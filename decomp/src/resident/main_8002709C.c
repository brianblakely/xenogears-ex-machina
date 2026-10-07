#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/inline_c.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "mode.h"
#include "gpu.h"
#include "menu.h"
#include "sprite.h"
#include "cd.h"
#include "stream.h"
#include "model.h"
#include "heap.h"
#include "text.h"
#include "pad.h"
#include "console.h"
#include "sound.h"

/* Create a panoramic backdrop (heap tag 4). `colours` (three RGB words:
 * sky, horizon, ground) enables the fills, NULL leaves them off. */
Panorama *func_8002709C(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y,
                        s32 mode, s32 turn, VECTOR *position, u8 *colours, u16 fill_scale,
                        u16 fade_range, u16 fade_start) {
    DRAWENV env;
    Panorama *panorama;
    POLY_FT4 *quad;
    s32 i;

    func_80032498(4, 0);
    panorama = func_80031BDC(sizeof(Panorama), 0);
    if (panorama == NULL) {
        return NULL;
    }
    GetDrawEnv(&env);
    panorama->width = width;
    panorama->height = height;
    panorama->vx = position->vx;
    panorama->vy = position->vy;
    panorama->vz = position->vz;
    panorama->fill_scale = fill_scale;
    panorama->fade_range = fade_range;
    panorama->fade_start = fade_start;
    if (position->vz >= 0) {
        panorama->turn = turn;
    } else {
        panorama->turn = -turn;
    }
    panorama->tex_x = tex_x;
    panorama->tex_y = tex_y;
    panorama->mode = mode;
    panorama->v = tex_y % 256;
    for (i = 0, quad = panorama->quads[0]; i < 16; i++, quad++) {
        SetPolyFT4(quad);
        SetShadeTex(quad, 1);
        quad->clut = GetClut(clut_x, clut_y);
    }
    if (colours != NULL) {
        panorama->fill = 1;
        for (i = 0; i < 2; i++) {
            SetPolyF4(&panorama->fills[i]);
            setRGB0(&panorama->fills[i], colours[0], colours[1], colours[2]);
            panorama->fills[i].x0 = 0;
            panorama->fills[i].y0 = 0;
            panorama->fills[i].x1 = 320;
            panorama->fills[i].y1 = 0;
            panorama->fills[i].x2 = 0;
            panorama->fills[i].x3 = 320;
        }
        colours += 4;
        for (i = 0; i < 2; i++) {
            setRGB0(&panorama->fades[i], colours[0], colours[1], colours[2]);
            setRGB1(&panorama->fades[i], colours[0], colours[1], colours[2]);
            colours += 4;
            setRGB2(&panorama->fades[i], colours[0], colours[1], colours[2]);
            setRGB3(&panorama->fades[i], colours[0], colours[1], colours[2]);
            colours -= 4;
            SetPolyG4(&panorama->fades[i]);
            panorama->fades[i].x0 = 0;
            panorama->fades[i].x1 = 320;
            panorama->fades[i].x2 = 0;
            panorama->fades[i].x3 = 320;
        }
        colours += 4;
        for (i = 2; i < 4; i++) {
            SetPolyF4(&panorama->fills[i]);
            setRGB0(&panorama->fills[i], colours[0], colours[1], colours[2]);
            panorama->fills[i].x0 = 0;
            panorama->fills[i].x1 = 320;
            panorama->fills[i].x2 = 0;
            panorama->fills[i].y2 = 240;
            panorama->fills[i].x3 = 320;
            panorama->fills[i].y3 = 240;
        }
    } else {
        panorama->fill = 0;
    }
    return panorama;
}

/* Draw a panoramic backdrop into `ot` for the view from `eye` to `target`:
 * the strip where its point ahead of the target projects (shrinking with
 * the distance past fade_start), and the sky, fade and ground fills around
 * it. Returns the strip's bottom screen row. */
s32 func_800273C4(Panorama *panorama, SVECTOR *eye, SVECTOR *target, MATRIX *view, u_long *ot,
                  s32 buffer) {
    DVECTOR bottom;
    DVECTOR edge;
    VECTOR d;
    SVECTOR p;
    SVECTOR n;
    s16 zoom;
    s16 y;

    if (panorama == NULL) {
        return 0;
    }
    d.vx = target->vx - eye->vx;
    d.vy = 0;
    d.vz = target->vz - eye->vz;
    VectorNormalS(&d, &n);
    p.vx = n.vx * panorama->vz / 4096 + target->vx;
    p.vy = panorama->vy;
    p.vz = n.vz * panorama->vz / 4096 + target->vz;
    SetRotMatrix(view);
    SetTransMatrix(view);
    gte_ldv0(&p);
    gte_rtps();
    gte_stsxy(&bottom);
    if (panorama->fade_range != 0) {
        d.vx = target->vx - eye->vx;
        d.vy = target->vy - eye->vy;
        d.vz = target->vz - eye->vz;
        zoom = (SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz) - panorama->fade_start) /
               panorama->fade_range;
        if (zoom < 0) {
            zoom = 0;
        }
        if (zoom > 0x100) {
            zoom = 0x100;
        }
    } else {
        zoom = 0;
    }
    func_800278F8(panorama, panorama->width * panorama->turn * (ratan2(d.vx, d.vz) & 0xFFF) / 4096,
                  bottom.vy, zoom, ot, buffer);
    if (panorama->fill > 0) {
        bottom.vx = bottom.vy - panorama->height;
        if (bottom.vx > 240) {
            bottom.vx = 240;
        }
        if (bottom.vx > 0) {
            panorama->fills[buffer].y2 = bottom.vx;
            panorama->fills[buffer].y3 = bottom.vx;
            addPrim(ot, &panorama->fills[buffer]);
        }
        p.vx = n.vx * panorama->vz / 4096 * panorama->fill_scale / 256 + target->vx;
        p.vy = panorama->vy * panorama->fill_scale / 256;
        p.vz = n.vz * panorama->vz / 4096 * panorama->fill_scale / 256 + target->vz;
        gte_ldv0(&p);
        gte_rtps();
        gte_stsxy(&edge);
        if (edge.vy - bottom.vy > 240) {
            edge.vy = bottom.vy + 240;
        }
        if (edge.vy >= 0 && bottom.vy < 240) {
            panorama->fades[buffer].y0 = bottom.vy;
            panorama->fades[buffer].y1 = bottom.vy;
            panorama->fades[buffer].y2 = edge.vy;
            panorama->fades[buffer].y3 = edge.vy;
            addPrim(ot, &panorama->fades[buffer]);
        }
        if (edge.vy < 0) {
            y = 0;
        } else {
            y = edge.vy;
        }
        if (y < 240) {
            panorama->fills[buffer + 2].y0 = y;
            panorama->fills[buffer + 2].y1 = y;
            addPrim(ot, &panorama->fills[buffer + 2]);
        }
    }
    return bottom.vy;
}

/* Draw the strip of a panoramic backdrop from texture column `start`,
 * `bottom` its bottom screen row, scaled down by `zoom` (8.8): up to eight
 * quads across the screen, one per texture page.
 * Nonmatching: the original keeps a second copy of `bottom` for the quads'
 * lower corners and saves `bottom`, `top` and `ot` around GetTPage. */
#ifdef NON_MATCHING
void func_800278F8(Panorama *panorama, s32 start, s32 bottom, s32 zoom, u_long *ot, s32 buffer) {
    s32 left;
    s16 u;
    s16 top;
    s16 x;
    s16 next;
    s16 w;
    s16 page_u;
    s16 page_x;
    s16 cols;
    s16 x1;
    s32 i;
    POLY_FT4 *quad;

    left = (320 - (panorama->width << 8) / (zoom + 0x100)) / 2;
    left += left * zoom / 256;
    u = (s16)(start - left) % panorama->width;
    if (u < 0) {
        u += panorama->width;
    }
    if (bottom < 0 || panorama->height + 240 < bottom) {
        top = 0;
    } else {
        top = (panorama->height << 8) / (zoom + 0x100);
    }
    x = 0;
    quad = panorama->quads[buffer & 1];
    if (top > 0) {
        page_u = (panorama->tex_x % 64) << (2 - panorama->mode);
        for (i = 0; i < 8; i++, quad++) {
            page_x = panorama->tex_x + (u >> (2 - panorama->mode));
            cols = (u + page_u) & ((0x100 >> panorama->mode) - 1);
            w = 0x100 - cols;
            if (u + w > panorama->width) {
                w = panorama->width - u;
            }
            x1 = (w << 8) / (zoom + 0x100);
            if (x + x1 > 320) {
                x1 = 320 - x;
                w = x1 * (zoom + 0x100) / 256;
            }
            next = (s16)(u + w) % panorama->width;
            quad->x0 = x;
            quad->x1 = x + x1;
            quad->x2 = x;
            quad->y2 = bottom;
            quad->x3 = x + x1;
            quad->y3 = bottom;
            quad->u0 = cols;
            quad->y0 = bottom - top;
            quad->y1 = bottom - top;
            quad->u1 = cols + w - 1;
            quad->v0 = panorama->v;
            quad->u2 = cols;
            quad->v1 = panorama->v;
            quad->u3 = cols + w - 1;
            quad->v2 = panorama->v + panorama->height;
            quad->v3 = panorama->v + panorama->height;
            u = next;
            quad->tpage = GetTPage(panorama->mode, 0, page_x / 64 * 64, panorama->tex_y / 256 * 256);
            addPrim(ot, quad);
            x += x1;
            if (x >= 320) {
                break;
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_800278F8);
#endif

/* Release a block if there is one. */
void func_80027D40(void *block) {
    if (block != NULL) {
        func_800320E8(block);
    }
}

/* Set up a texture scroll of `count` bands over an area; the band phases are allocated (heap tag 4) and cleared. Returns the scroll, or NULL. */
TextureScroll *func_80027D64(TextureScroll *scroll, s16 x, s16 y, s16 w, s16 h, s16 count, u16 source_x,
                             u16 source_y, s8 *speeds) {
    s32 i;

    func_80032498(4, 0);
    scroll->x = x;
    scroll->y = y;
    scroll->w = w;
    scroll->h = h;
    scroll->step = h / count;
    scroll->count = count;
    scroll->source_x = source_x;
    scroll->source_y = source_y;
    scroll->speeds = speeds;
    scroll->phases = func_80031BDC(count * 2, 0);
    if (scroll->phases == NULL) {
        scroll = NULL;
    } else {
        for (i = 0; i < count; i++) {
            scroll->phases[i] = 0;
        }
    }
    return scroll;
}

/* Advance each band's phase by its speed and redraw the area rotated by it (two MoveImage copies per band). */
void func_80027EAC(TextureScroll *scroll) {
    RECT rect;
    s32 i;
    u16 line;
    u16 offset;
    u16 rest;

    if (scroll->phases == NULL) {
        return;
    }
    rect.y = scroll->y;
    rect.h = scroll->step;
    line = scroll->source_y;
    for (i = 0; i < scroll->count; i++) {
        scroll->phases[i] += scroll->speeds[i];
        offset = (u16)((s16)scroll->phases[i] >> 4) % scroll->w;
        rest = scroll->w - offset;
        rect.x = scroll->x;
        rect.w = offset;
        MoveImage(&rect, scroll->source_x + rest, line);
        rect.x = offset + scroll->x;
        rect.w = rest;
        MoveImage(&rect, scroll->source_x, line);
        line += scroll->step;
        rect.y += scroll->step;
    }
}

/* Release a texture scroll's phases. */
void func_8002800C(TextureScroll *scroll) {
    if (scroll->phases != NULL) {
        func_800320E8(scroll->phases);
        scroll->phases = NULL;
    }
}

/* Disc error indicator: draw a coloured bar for retry `level`; from the fourth, switch to a text screen showing the failing file forever. */
void func_8002804C(s32 level, s32 r, s32 g, s32 b) {
    RECT rect;
    struct {
        DRAWENV draw;
        DISPENV disp;
        u_long ot[16];
    } screen;

    rect.x = level * 10 + 10;
    rect.w = 8;
    rect.y = 0;
    rect.h = 0x1C0;
    if (level + 1 >= 4) {
        r = 0xFF;
        g = 0xFF;
        b = 0xFF;
    }
    ClearImage(&rect, r, g, b);
    DrawSync(0);
    if (level + 1 >= 4) {
        ResetGraph(0);
        InitGeom();
        SetDefDrawEnv(&screen.draw, 0, 0, 0x140, 0x100);
        SetDefDispEnv(&screen.disp, 0, 0, 0x140, 0xF0);
        screen.draw.dtd = 0;
        screen.draw.isbg = 0;
        screen.draw.dfe = 1;
        screen.disp.isinter = 0;
        PutDrawEnv(&screen.draw);
        PutDispEnv(&screen.disp);
        func_800374E8(0x10, 0x10, 0x280, 0xF0, 0x400, 0, 0x280, 0, 0x280, 0x100, 0);
        SetDispMask(1);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x280;
        rect.h = 0x30;
        ClearImage(&rect, 0, 0, 0);
        for (;;) {
            ClearOTagR(screen.ot, 8);
            func_8003700C("\n%d", D_8004FE14 + D_80059F0C - 1);
            func_8003700C("\n%s", func_80028998(D_80059F0C));
            func_80037324(screen.ot);
            DrawOTag(&screen.ot[7]);
            DrawSync(0);
        }
    }
}

/* Initialise disc access: the CD library (or the PC file server for other modes), the file index and directory table, reading both from sectors 24 and 40 when booting from CD. */
void func_80028230(u8 *files, u16 *directories, u32 mode) {
    D_8005A488 = 0;
    D_8005A48C = 0;
    D_8005A490 = 0;
    D_8005A494 = 0;
    D_8005A498 = 0;
    D_8005A49C = 0;
    D_8005A4A4 = 0;
    D_8005A4A8 = 0;
    D_8005A4B4 = 0;
    if (mode == 0 || mode == -1) {
        while (CdInit() == 0) {
        }
        CdSetDebug(0);
        CdDataCallback(0);
        CdSyncCallback(0);
        CdReadyCallback(0);
        CdControl(7, 0, D_80059F1C);
        func_8002A428(0xA0);
        func_80028A60(0);
        VSync(3);
    } else {
        func_8004C38C();
    }
    if (mode != -1) {
        D_8004FE48 = (char *)mode;
    } else {
        D_8004FE48 = NULL;
    }
    D_8004FDF0 = files;
    D_8004FDF4 = directories;
    D_8004FE14 = 0;
    D_8004FDFC = 0;
    D_8004FDF8 = 0;
    D_8004FE1C = 0;
    D_8004FE4C = -1;
    if (mode == 0) {
        func_8002954C(0x18, files, 0x8000, 0, 0);
        func_80028A60(0);
        func_8002954C(0x28, D_8004FDF4, 0x7A, 0, 0);
        func_80028A60(0);
    }
}

/* End disc access: stop the read, pause the drive (on CD) and clear the CD callbacks. */
void func_800283D4(void) {
    func_8002A498(0);
    func_80028A60(0);
    if (D_8004FE48 == NULL) {
        while (CdControlB(9, 0, D_80059F1C) == 0) {
        }
        func_8002A428(0xA0);
        func_80028A60(0);
        VSync(3);
    }
    CdDataCallback(0);
    CdSyncCallback(0);
    CdReadyCallback(0);
    D_8004FDFC = 0;
    D_8004FDF8 = 0;
    D_8004FE1C = 0;
}

/* Select a directory by group and index in the directory table; returns it, or -1 (selecting 0) when that entry is empty. */
s32 func_80028470(s32 group, s32 index) {
    D_8004FE14 = D_8004FDF4[group + index] - 1;
    if (D_8004FE14 < 0) {
        D_8004FE14 = 0;
        return -1;
    }
    return D_8004FE14;
}

/* The directory group (a multiple of four) and index of the selected directory, both zero when none matches; returns the selection. */
s32 func_800284B4(s32 *group, s32 *index) {
    u16 *entry = D_8004FDF4;
    s32 i;

    for (i = 0; i < 0x40; i++, entry++) {
        if (*entry == D_8004FE14 + 1) {
            *group = i / 4 * 4;
            *index = i - i / 4 * 4;
            break;
        }
    }
    if (i == 0x40) {
        *group = 0;
        *index = 0;
    }
    return D_8004FE14;
}

/* The disc number (directory table word 0x3c). */
s32 func_80028530(void) {
    return D_8004FDF4[0x3C];
}

/* The directory at group + index relative to the selected one. */
s32 func_80028548(s32 group, s32 index) {
    return D_8004FDF4[group + index] - D_8004FE14;
}

/* Load a whole PC file into a new heap block (four tries per file-server call); returns the block, or NULL. */
void *func_80028570(char *name, s32 *size) {
    s32 fd;
    s32 length;
    s32 read;
    s32 i;
    void *block;

    for (i = 0; i < 4; i++) {
        fd = PCopen(name, 0, 0);
        if (fd != -1) {
            break;
        }
    }
    if (fd != -1) {
        length = PClseek(fd, 0, 2);
        if (size != NULL) {
            *size = length;
        }
        PClseek(fd, 0, 0);
        block = func_80031BDC(length, 0);
        read = 0;
        if (block != NULL) {
            for (i = 0; i < 4; i++) {
                read = func_8004C398(fd, block, length);
                if (read != 0) {
                    break;
                }
            }
        }
        if (read == 0) {
            if (block != NULL) {
                func_800320E8(block);
            }
            block = NULL;
        }
        for (i = 0; i < 4; i++) {
            if (PCclose(fd) == 0) {
                goto done;
            }
        }
        if (block != NULL) {
            func_800320E8(block);
        }
    }
    block = NULL;
done:
    return block;
}

s32 func_800286BC(void) {
    return D_8004FDF8;
}

/* Nonzero while a disc read is pending, the drive is busy or a read is in progress. */
s32 func_800286CC(void) {
    s32 state = D_8004FDFC;

    if (state == 0) {
        if (D_8004FE48 == NULL && CdDataSync(1) != 0) {
            return 1;
        }
        if (D_8004FE1C != 0) {
            return 1;
        }
    }
    return state;
}

/* A file's byte size: from the PC file server when it knows the file, else from the file index. */
s32 func_80028738(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (D_8004FE48 != NULL) {
        fd = PCopen(func_80028998(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
        if (size > 0) {
            goto done;
        }
    }
    entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
done:
    return size;
}

/* As 80028738 in the second directory selection (8004fe18), rounded up to words. */
s32 func_80028808(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (D_8004FE48 != NULL) {
        fd = PCopen(func_80028998(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
        if (size > 0) {
            return (size + 3) / 4 * 4;
        }
    }
    entry = &D_8004FDF0[(file + D_8004FE18 - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
    return (size + 3) / 4 * 4;
}

/* A file's byte size rounded up to words. */
s32 func_800288EC(s32 file) {
    s32 size = func_80028738(file);

    return (size + 3) / 4 * 4;
}

/* For an index entry with a negative size (a directory), its file count; 0 for a file. */
s16 func_80028928(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];
    s32 size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];

    if (size < 0) {
        return -size;
    }
    return 0;
}

/* The PC file server name of a file (64 bytes per file), or NULL without the server. */
char *func_80028998(s32 file) {
    char *name = NULL;

    if (D_8004FE48 != NULL) {
        name = D_8004FE48 + (file + D_8004FE14 - 1) * 64;
    }
    return name;
}

/* A file's first sector in the selected directory. */
s32 func_800289D0(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}

/* A file's first sector in the second directory selection (8004fe18). */
s32 func_80028A18(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE18 - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}

/* Wait for the disc (mode 0: until idle); returns the disc status. */
s32 func_80028A60(s32 mode) {
    if (mode == 0) {
        while (func_800286CC() > 0) {
        }
    }
    return func_800286CC();
}

/* Replace the shared ring and return the previous one. */
StreamRing *func_80028A94(StreamRing *ring) {
    StreamRing *previous = D_8004FE30;
    D_8004FE30 = ring;
    return previous;
}

/* Clear every slot; the first slot's third halfword (ring offset 8) keeps
 * the low count. Returns the count, or -1 without a ring. */
s32 func_80028AAC(void) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 i;
    s32 count;

    if (ring == NULL) {
        return -1;
    }
    count = ring->count;
    slots = ring->slots;
    for (i = 0; i < count; i++) {
        slots[i].state = 0;
        slots[i].sequence = 0;
        slots[i].length = 0;
        slots[i].w6 = 0;
    }
    slots->length = count;
    return count;
}

/* The next chunk of the stream, or NULL. With the PC file server, read the
 * next sector of the file into a free slot (opening the list's next file
 * when this one ends); from the disc, take the slot the CD callbacks filled
 * for the next sequence number. */
u8 *func_80028B14(void) {
    StreamRing *ring = D_8004FE30;
    u8 *payload;
    StreamSlot *slot;
    FileRequest *request;
    s32 count;
    s32 index;
    s32 i;
    s32 file;
    char *name;

    if (ring == NULL) {
        return NULL;
    }
    payload = (u8 *)ring;
    slot = ring->slots;
    count = ring->count;
    payload = payload + count * 8 + 0x24;
    if (D_8004FE48 != NULL) {
        if (D_8004FE4C == -1) {
            return NULL;
        }
        if (D_8004FDF8 <= 0) {
            return NULL;
        }
        for (i = 0; i < D_8004FE40; i++) {
            slot = &D_8004FE2C[D_8004FE10];
            index = D_8004FE10;
            if (++D_8004FE10 >= D_8004FE40) {
                D_8004FE10 = 0;
            }
            if (slot->state == 0) {
                goto found;
            }
        }
        if (slot->state != 0) {
            return NULL;
        }
    found:
        slot->state = 3;
        payload += index << 11;
        for (i = 0; i < 4; i++) {
            if (func_8004C398(D_8004FE4C, payload, 0x800) != 0) {
                goto read;
            }
            func_8002804C(i, 0, 0xFF, 0);
        }
        return NULL;
    read:
        D_8004FDF8 -= 0x800;
        if (D_8004FDF8 > 0) {
            return payload;
        }
        D_8004FDF8 = 0;
        for (i = 0; i < 4; i++) {
            if (PCclose(D_8004FE4C) == 0) {
                break;
            }
            func_8002804C(i, 0, 0, 0xFF);
        }
        D_8004FE4C = -1;
        if (D_8004FE0C != NULL) {
            request = &D_8004FE0C[++D_8004FE10];
            file = request->file;
            D_80059F0C = file;
            if (file > 0 && request->destination != NULL) {
                name = func_80028998(file);
                for (i = 0; i < 4; i++) {
                    D_8004FE4C = PCopen(name, 0, 0);
                    if (D_8004FE4C != -1) {
                        break;
                    }
                    func_8002804C(i, 0xFF, 0, 0);
                }
                D_8004FDF8 = func_80028808(file);
                D_8004FDFC--;
                return payload;
            }
            D_8004FDF8 = 0;
        }
        D_8004FDFC = 0;
        return payload;
    }
    for (i = 0; i < count; i++, slot++) {
        if (slot->state == 3 && slot->sequence == D_8004FE24) {
            break;
        }
    }
    if (i == D_8004FE40) {
        return NULL;
    }
    D_8004FE24++;
    return payload + (i << 11);
}

/* Whether any of `count` ring slots from `index` differs from `state` or runs past the last slot. */
s32 func_80028E60(s32 index, s32 count, s32 state) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (D_8004FE2C[index].state != state) {
            return 1;
        }
        if (++index > D_8004FE40) {
            return 1;
        }
    }
    return 0;
}

/* Merge the free run that follows slot `index` into its length. */
void func_80028ECC(s32 index) {
    s16 length = D_8004FE2C[index].length;
    s32 next = index + length;

    if (next < D_8004FE40 && D_8004FE2C[next].state == 0) {
        D_8004FE2C[index].length = length + D_8004FE2C[next].length;
    }
}

/* The next complete movie frame: its first sector header in *frame and its
 * data in *data (0 when one is available, 1 otherwise). With the PC file
 * server, first read the next sector: a frame's first sector reserves a free
 * run of slots for all its sectors, later sectors fill them. */
/* Nonmatching: close; the original reads the CD mode byte without keeping its address in a
 * register (as a scalar, where 8002a428's unit uses an array), the payload and slot-offset
 * registers ($s5/$s4) are swapped, and the frame size is reloaded after the slot stores. */
#ifdef NON_MATCHING
s32 func_80028F30(u8 **data, StreamFrame **frame) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    StreamSlot *slot;
    StreamFrame *header;
    u8 *payload;
    s32 count;
    s32 offset;
    s32 rest;
    s32 i;

    if (ring == NULL) {
        return 1;
    }
    payload = (u8 *)ring;
    slots = ring->slots;
    count = ring->count;
    payload = payload + count * 8 + 0x24;
    if (D_8004FE48 != NULL && D_8004FE4C != -1 && D_8004FDF8 > 0) {
        if (D_80059F60 == 0) {
            for (i = 0; i < D_8004FE40; i += slot->length) {
                slot = &D_8004FE2C[i];
                if (slot->state == 0) {
                    break;
                }
            }
            if (i >= D_8004FE40) {
                goto search;
            }
            offset = i << 11;
            if (D_80059F18[0] & 8) {
                func_8004C398(D_8004FE4C, D_800596F8, 8);
                if (D_800596F8[0] == 1) {
                    goto skip;
                }
            }
            D_80059F54 = (u8 *)D_8004FE08 + offset;
            header = (StreamFrame *)D_80059F54;
            func_8004C398(D_8004FE4C, (u8 *)header, 0x20);
            D_80059F5C = header->sectors;
            D_8005A4B8 = header->word8;
            if (slot->length < D_80059F5C) {
                if (D_80059F18[0] & 8) {
                    PClseek(D_8004FE4C, -0x28, 1);
                } else {
                    PClseek(D_8004FE4C, -0x20, 1);
                }
                *frame = NULL;
                goto search_all;
            }
            slot->state = 3;
            slot->sequence = D_8004FE26;
            rest = slot->length - D_80059F5C;
            if (rest >= 3) {
                slot->length = D_80059F5C + 1;
                slot[D_80059F5C + 1].length = rest - 1;
                slot[D_80059F5C + 1].state = 0;
                func_80028ECC(D_80059F5C + 1);
            }
            D_8004FE26++;
            D_80059F58 = (u8 *)D_8004FE08 + offset + D_80059F5C * 32;
            func_8004C398(D_8004FE4C, D_80059F58, 0x7E0);
            if (D_80059F18[0] & 8) {
                PClseek(D_8004FE4C, 0x118, 1);
            }
            D_8004FE10 = i;
            D_8004FDF8 -= 0x800;
            D_80059F60++;
        } else {
            if (D_80059F18[0] & 8) {
                func_8004C398(D_8004FE4C, D_800596F8, 8);
                if (D_800596F8[0] == 1) {
                skip:
                    PClseek(D_8004FE4C, 0x918, 1);
                    *frame = NULL;
                    goto search_all;
                }
            }
            slot = &D_8004FE2C[++D_8004FE10];
            slot->state = 3;
            slot->sequence = D_8004FE26++;
            func_8004C398(D_8004FE4C, D_80059F54 + D_80059F60 * 32, 0x20);
            func_8004C398(D_8004FE4C, D_80059F58 + D_80059F60 * 0x7E0, 0x7E0);
            if (D_80059F18[0] & 8) {
                PClseek(D_8004FE4C, 0x118, 1);
            }
            D_8004FDF8 -= 0x800;
            if (++D_80059F60 >= D_80059F5C) {
                D_80059F60 = 0;
            }
        }
    }
search:
    *frame = NULL;
search_all:
    for (i = 0; i < count; i++, slots++) {
        if (slots->state == 3 && slots->sequence == D_8004FE24) {
            break;
        }
    }
    if (i == D_8004FE40) {
        return 1;
    }
    header = (StreamFrame *)(payload + (i << 11));
    *frame = header;
    *data = (u8 *)header + header->sectors * 32;
    if (func_80028E60(i, header->sectors, 3) != 0) {
        return 1;
    }
    D_8004FE24 += header->sectors;
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_80028F30);
#endif

/* Release a ring chunk: clear its slot's state and return the old state (0xffff without a ring, 0 for no chunk). */
u16 func_8002945C(u8 *chunk) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 index;
    u16 state;
    u8 *payload;

    if (ring == NULL) {
        return 0xFFFF;
    }
    if (chunk == NULL) {
        return 0;
    }
    slots = ring->slots;
    payload = (u8 *)ring + ring->count * 8 + 0x24;
    index = (u32)(chunk - payload) >> 11;
    state = slots[index].state;
    slots[index].state = 0;
    return state;
}

/* Release a run of ring chunks (the chunk header's halfword 3 counts them), merge the freed run and return the first slot's old state. */
u16 func_800294B4(u8 *chunk) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 index;
    s32 i;
    u16 state;
    u8 *payload;

    if (ring == NULL) {
        return 0xFFFF;
    }
    slots = ring->slots;
    if (chunk == NULL) {
        return 0;
    }
    i = ((u16 *)chunk)[3];
    payload = (u8 *)ring + ring->count * 8 + 0x24;
    index = (u32)(chunk - payload) >> 11;
    state = slots[index].state;
    for (; i > 0; i--) {
        slots[index + i - 1].state = 0;
    }
    func_80028ECC(index);
    return state;
}

/* Read `size` bytes from a raw disc sector (CD only); -1 with the PC file server. */
s32 func_8002954C(s32 sector, void *destination, s32 size, s32 a3, s32 a4) {
    if (D_8004FE48 != NULL) {
        return -1;
    }
    func_80028A60(0);
    D_8004FE04 = sector;
    D_8004FDF8 = size;
    return func_80029690(0, destination, a3, a4);
}

/* Read a file of the selected directory; -3 for an invalid file, an empty file or no destination. */
s32 func_800295D8(s32 file, void *destination, s32 a2, s32 a3) {
    if (file <= 0 || func_80028738(file) <= 0 || destination == NULL) {
        return -3;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_800288EC(file);
    return func_80029690(file, destination, a2, a3);
}

/* Start reading `D_8004FDF8` bytes from sector D_8004FE04: into a stream ring (flags 0x100, or 0x200 with CD mode byte flags | 0xa0) or into memory; with the PC file server the file is opened (ring) or read now. */
/* Keep the ring count read at setup while the callback globals are written.
 * Nonmatching: under the qualified GCC 2.6.3 / ASPSX 2.34 pipeline, the
 * original loads the data callback address before storing the read-active
 * flag; this C reverses those two pairs of instructions (four differences). */
#ifdef NON_MATCHING
s32 func_80029690(s32 file, void *destination, s32 mode, s32 flags) {
    StreamRing *ring;
    s32 fd;
    s32 count;
    s32 i;
    CdlLOC *position;
    u8 *mode_byte;

    D_80059F0C = file;
    for (i = 2; i >= 0; i--) {
        D_80059EF8[i] = 0;
    }
    position = &D_80059F10;
    D_8004FDFC = 1;
    D_8004FE08 = destination;
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE10 = 0;
    D_8004FE0C = NULL;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    CdIntToPos(D_8004FE04, position);
    if (flags & 0x100) {
        func_80028A94(destination);
        ring = D_8004FE30;
        count = ring->count;
        if (count == 0) {
            return -4;
        }
        D_8004FE08 = (u8 *)ring + count * 8 + 0x24;
        D_8004FE2C = ring->slots;
        D_8004FE40 = count;
        D_8004FE26 = 0;
        D_8004FE28 = 0;
        D_8004FE24 = 0;
        func_80028AAC();
        if (D_8004FE48 != NULL) {
            char *name = func_80028998(file);
            for (i = 0; i < 4; i++) {
                D_8004FE4C = PCopen(name, 0, 0);
                if (D_8004FE4C != -1) {
                    break;
                }
                func_8002804C(i, 0xFF, 0, 0);
            }
            return D_8004FE4C == -1 ? -3 : 0;
        }
        D_8004FE1C = 1;
        CdDataCallback(func_8002BA58);
        CdSyncCallback(func_8002A68C);
        CdReadyCallback(func_8002B2F0);
    } else if (flags & 0x200) {
        char *name;

        func_80028A94(destination);
        ring = D_8004FE30;
        count = ring->count;
        if (count == 0) {
            return -4;
        }
        D_8004FE08 = (u8 *)ring + count * 8 + 0x24;
        D_8004FE2C = ring->slots;
        D_8004FE40 = count;
        D_80059F60 = 0;
        D_8004FE26 = 0;
        D_8004FE28 = 0;
        D_8004FE24 = 0;
        func_80028AAC();
        for (i = 3, mode_byte = &D_80059F18[3]; i >= 0; i--) {
            *mode_byte-- = 0;
        }
        D_80059F18[0] = flags | 0xA0;
        if (D_8004FE48 == NULL) {
            return 0;
        }
        name = func_80028998(file);
        for (i = 0; i < 4; i++) {
            D_8004FE4C = PCopen(name, 0, 0);
            if (D_8004FE4C != -1) {
                break;
            }
            func_8002804C(i, 0xFF, 0, 0);
        }
        return D_8004FE4C == -1 ? -3 : 0;
    } else {
        if (D_8004FE48 != NULL) {
            char *name = func_80028998(file);
            for (i = 0; i < 4; i++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                func_8002804C(i, 0xFF, 0, 0);
            }
            if (fd == -1) {
                return -4;
            }
        opened:
            if (destination != NULL) {
                for (i = 0; i < 4; i++) {
                    if (func_8004C398(fd, destination, D_8004FDF8) != 0) {
                        break;
                    }
                    func_8002804C(i, 0, 0xFF, 0);
                }
            }
            for (i = 0; i < 4; i++) {
                if (PCclose(fd) == 0) {
                    D_8004FDF8 = 0;
                    D_8004FDFC = 0;
                    return 0;
                }
                func_8002804C(i, 0, 0, 0xFF);
            }
            return -6;
        }
        D_8004FE1C = 1;
        CdDataCallback(NULL);
        CdSyncCallback(func_8002A68C);
        CdReadyCallback(func_8002B084);
    }
    D_8005A488++;
    CdControlF(2, (u8 *)position);
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_80029690);
#endif

/* Read a zero-terminated file list: sort it by file, then start the CD reads (the callbacks continue them), or with the PC file server read every file now. Returns 0, or -3 for an empty list. */
s32 func_80029AFC(FileRequest *list, s32 mode, s32 unused) {
    s32 count;
    s32 i;
    s32 j;
    s32 best;
    u16 file;
    void *destination;
    char *name;
    s32 fd;

    if (list == NULL) {
        return -3;
    }
    for (count = 0; list[count].file != 0; count++) {
    }
    if (count == 0) {
        return -3;
    }
    for (i = 0; i < count - 1; i++) {
        file = list[i].file;
        best = i;
        for (j = i + 1; j < count; j++) {
            if (list[j].file < file) {
                best = j;
                file = list[j].file;
            }
        }
        file = list[i].file;
        destination = list[i].destination;
        list[i].file = list[best].file;
        list[i].destination = list[best].destination;
        list[best].file = file;
        list[best].destination = destination;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    for (i = 2; i >= 0; i--) {
        D_80059EF8[i] = 0;
    }
    D_8004FE10 = 0;
    D_8004FE0C = list;
    D_8004FDFC = count;
    D_8004FE00 = count;
    D_8004FE08 = list->destination;
    file = list->file;
    if (file == 0 || D_8004FE08 == NULL) {
        func_8002A394(mode);
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        return 0;
    }
    D_80059F0C = file;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_80028808(file);
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE3C = 0;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8004FE48 != NULL) {
        for (i = 0; i < count; i++) {
            file = list[i].file;
            D_80059F0C = file;
            name = func_80028998(file);
            for (j = 0; j < 4; j++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                func_8002804C(j, 0xFF, 0, 0);
            }
            goto close;
        opened:
            if (list[i].destination != NULL) {
                for (j = 0; j < 4; j++) {
                    if (func_8004C398(fd, list[i].destination, func_80028808(file)) != 0) {
                        break;
                    }
                    func_8002804C(j, 0, 0xFF, 0);
                }
            }
        close:
            for (j = 0; j < 4; j++) {
                if (PCclose(fd) == 0) {
                    break;
                }
                func_8002804C(j, 0, 0, 0xFF);
            }
        }
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        return 0;
    }
    D_8004FE1C = 1;
    CdDataCallback(func_8002BA40);
    CdSyncCallback(func_8002A68C);
    CdReadyCallback(func_8002AC24);
    D_8005A488++;
    CdControlF(2, (u8 *)&D_80059F10);
    return 0;
}

/* Start streaming a file through a ring of at least two slots with six stream parameters; with the PC file server the whole stream is pumped now. Returns 0, -3 for a bad file, -4 for a bad ring, -6 when the file does not close. */
s32 func_80029EB0(s32 file, StreamRing *ring, s32 mode, s32 unused, u16 a, u16 b, u16 c, u16 d, u16 e, u16 f) {
    s32 count;
    char *name;
    s16 i;

    if (ring == NULL || (u32)(count = ring->count) < 2) {
        return -4;
    }
    if (file <= 0) {
        return -3;
    }
    if (func_80028738(file) <= 0) {
        return -3;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    for (i = 0; i < 3; i++) {
        D_80059EF8[i] = 0;
    }
    func_80028A94(ring);
    D_80059F0C = file;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_800288EC(file);
    D_8004FDFC = 1;
    D_8004FE08 = (u8 *)ring + count * 8 + 0x24;
    D_8004FE2C = ring->slots;
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE10 = 0;
    D_8004FE40 = count;
    D_8004FE26 = 0;
    D_8004FE28 = 0;
    D_8004FE0C = NULL;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    D_80059F24 = a;
    D_80059F28 = b;
    D_80059F2C = c;
    D_80059F30 = d;
    D_80059F34 = e;
    D_80059F38 = f;
    D_80059F3C = 0;
    D_80059F40 = 0;
    D_80059F44 = 0;
    D_80059F48 = 0;
    D_80059F4C = 0;
    D_80059F50 = 0;
    func_80028AAC();
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8004FE48 != NULL) {
        name = func_80028998(file);
        for (i = 0; i < 4; i++) {
            D_80059F04 = PCopen(name, 0, 0);
            if (D_80059F04 != -1) {
                break;
            }
            func_8002804C(i, 0xFF, 0, 0);
        }
        do {
            func_8002B8B0(0, 0);
            func_8002BF38(0, 0);
        } while (D_8004FDFC > 0);
        for (i = 0; i < 4; i++) {
            i = PCclose(D_80059F04);
            if (i == 0) {
                break;
            }
            func_8002804C(i, 0, 0, 0xFF);
        }
        if (i != 0) {
            return -6;
        }
        D_8004FDFC = 0;
        D_8004FDF8 = 0;
        return 0;
    }
    D_8004FE1C = 1;
    CdDataCallback(func_8002BB50);
    CdSyncCallback(func_8002A68C);
    CdReadyCallback(func_8002B5D0);
    D_8005A488++;
    CdControlF(2, (u8 *)&D_80059F10);
    return 0;
}
