/* Panoramic backdrops, texture scrolls, disc access and streams
 * (8002709c-8002c3e8), GCC 2.6.3 with inline division checks (80027d64,
 * 80027eac, 80028808 and 80029afc match only so, 8002bb50 only under
 * 2.6.3): the backdrops and scrolls, the PC file server's retry screen, disc
 * start-up, directories and file sizes, the shared sector ring, file, list
 * and stream reads with their CD command and data callbacks (and the PC file
 * server's), and the image stream steps. Its code from 8002a260 on has no
 * division, but it reads the same statics as the code before it, so the unit
 * runs to the model unit (model_renderer.c), whose jump table phase starts a
 * unit after 8002c310. Its .data (0x8004fde0) is the disc and stream state. */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/inline_c.h"
#include "psyq/libsn.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/stream.h"
#include "own_declarations.h"

/* This unit's own statics, which functions on both sides of 8002a260 read:
 * its one .bss, in declaration order (800596f8-80059f64, among the units'
 * larger variables). */
static u8 cd_sector_buffer[0x800];  /* 800596F8: sector buffer; the PC file server's subheader opens it */
static s32 cd_sector_header[3];     /* 80059EF8: read status words */
static s32 stream_image_pc_file_descriptor;        /* 80059F04: PC file server handle of the stream */
static CdlCB cd_saved_ready_callback;      /* 80059F08: ready callback saved while retrying */
static s32 cd_file_being_read;        /* 80059F0C: the file being read */
static CdlLOC cd_setloc_parameter;     /* 80059F10: CD position of the current read */
static CdlFILTER cd_setfilter_parameter;  /* 80059F14: CdlSetfilter parameter */
/* The CD mode byte, the first of the 4-byte CdlSetmode parameter, which
 * takes the whole word slot (decomp/Makefile, slots). A compromise: 80029690
 * and 8002a428 clear all four bytes through &cd_setmode_parameter + 3, beyond the
 * declared byte, and pass it as the parameter, while 80028f30's tests match
 * only with a u8 scalar (a u8[4], a union of the byte and the four, or a word
 * read through a u8 lvalue each make GCC keep the address in a register: one
 * more saved register, a 0x58-byte frame). The byte lies among this unit's
 * declaration-ordered statics, not among the commons, so all three functions
 * are this unit's and see one declaration. */
static u8 cd_setmode_parameter; /* 80059F18 */
static u8 cd_command_result[8];      /* 80059F1C: CD command result */
/* Image stream parameters set by 80029eb0: for images of type 0x1200 and
 * 0x1201, a placement mode (1: base + offset, 2: base + origin + offset,
 * otherwise origin + offset) and a base position. */
static s16 stream_image_1200_mode; /* 80059F24 */
static u16 stream_image_1200_base_x; /* 80059F28 */
static u16 stream_image_1200_base_y; /* 80059F2C */
static s16 stream_image_1201_mode; /* 80059F30 */
static u16 stream_image_1201_base_x; /* 80059F34 */
static u16 stream_image_1201_base_y; /* 80059F38 */
static s32 stream_image_remaining_count;        /* 80059F3C: images left in the stream */
static s16 stream_image_strip_x;        /* 80059F40: next strip: x */
static s16 stream_image_strip_y;        /* 80059F44: y */
static s16 stream_image_strip_width;        /* 80059F48: width */
static u16 *stream_image_strip_heights;       /* 80059F4C: heights of the remaining strips */
static s32 stream_image_remaining_strip_count;        /* 80059F50: strips left in the current image */
static u8 *stream_frame_headers;        /* 80059F54: sector headers of the frame being read */
static u8 *stream_frame_payloads;        /* 80059F58: payloads of the frame being read */
static s16 stream_frame_sector_count;        /* 80059F5C: sectors of the frame being read */
static s16 stream_frame_sector_index;        /* 80059F60: sector of the frame being read from the PC file server */

/* 8002709C: Create a panoramic backdrop (heap tag 4). `colours` (three RGB words:
 * sky, horizon, ground) enables the fills, NULL leaves them off. */
Panorama *gpu_create_panorama(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y,
                        s32 mode, s32 turn, VECTOR *position, u8 *colours, u16 fill_scale,
                        u16 fade_range, u16 fade_start) {
    DRAWENV env;
    Panorama *panorama;
    POLY_FT4 *quad;
    s32 i;

    heap_select_owner_tag(4, 0);
    panorama = heap_alloc(sizeof(Panorama), 0);
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

/* 800273C4: Draw a panoramic backdrop into `ot` for the view from `eye` to `target`:
 * the strip where its point ahead of the target projects (shrinking with
 * the distance past fade_start), and the sky, fade and ground fills around
 * it. Returns the strip's bottom screen row. */
s32 gpu_draw_panorama(Panorama *panorama, SVECTOR *eye, SVECTOR *target, MATRIX *view, u_long *ot,
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
    gpu_draw_panorama_strip(panorama, panorama->width * panorama->turn * (ratan2(d.vx, d.vz) & 0xFFF) / 4096,
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

/* 800278F8: Draw the strip of a panoramic backdrop from texture column `start`,
 * `bottom` its bottom screen row, scaled down by `zoom` (8.8): up to eight
 * quads across the screen, one per texture page. The quads' lower row is
 * an s16 copy of `bottom` taken in the loop. */
void gpu_draw_panorama_strip(Panorama *panorama, s32 start, s32 bottom, s32 zoom, u_long *ot, s32 buffer) {
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
    s16 y;
    s32 tex_x;
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
        tex_x = panorama->tex_x;
        page_u = (tex_x % 64) << (2 - panorama->mode);
        for (i = 0; i < 8; quad++, i++) {
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
            y = bottom;
            quad->x0 = x;
            quad->x1 = x + x1;
            quad->x2 = x;
            quad->y2 = y;
            quad->x3 = x + x1;
            quad->y3 = y;
            quad->y0 = bottom - top;
            quad->y1 = bottom - top;
            quad->u0 = cols;
            quad->v0 = panorama->v;
            quad->u1 = cols + w - 1;
            quad->v1 = panorama->v;
            quad->u2 = cols;
            quad->v2 = panorama->v + panorama->height;
            quad->u3 = cols + w - 1;
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

/* 80027D40: Release a block if there is one. */
void gpu_free_panorama(void *block) {
    if (block != NULL) {
        heap_free(block);
    }
}

/* 80027D64: Set up a texture scroll of `count` bands over an area; the band phases are allocated (heap tag 4) and cleared. Returns the scroll, or NULL. */
TextureScroll *gpu_init_texture_scroll(TextureScroll *scroll, s16 x, s16 y, s16 w, s16 h, s16 count, u16 source_x,
                             u16 source_y, s8 *speeds) {
    s32 i;

    heap_select_owner_tag(4, 0);
    scroll->x = x;
    scroll->y = y;
    scroll->w = w;
    scroll->h = h;
    scroll->step = h / count;
    scroll->count = count;
    scroll->source_x = source_x;
    scroll->source_y = source_y;
    scroll->speeds = speeds;
    scroll->phases = heap_alloc(count * 2, 0);
    if (scroll->phases == NULL) {
        scroll = NULL;
    } else {
        for (i = 0; i < count; i++) {
            scroll->phases[i] = 0;
        }
    }
    return scroll;
}

/* 80027EAC: Advance each band's phase by its speed and redraw the area rotated by it (two MoveImage copies per band). */
void gpu_update_texture_scroll(TextureScroll *scroll) {
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

/* 8002800C: Release a texture scroll's phases. */
void gpu_free_texture_scroll(TextureScroll *scroll) {
    if (scroll->phases != NULL) {
        heap_free(scroll->phases);
        scroll->phases = NULL;
    }
}

/* 8002804C: The PC file server's retry and error screen; only the host-file paths call
 * it. Draw a coloured bar for retry `level` (red after a failed open, green
 * after a read, blue after a close); from the fourth, switch to a text screen
 * showing the failing file forever. */
void cd_draw_error_indicator(s32 level, s32 r, s32 g, s32 b) {
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
        console_open(0x10, 0x10, 0x280, 0xF0, 0x400, 0, 0x280, 0, 0x280, 0x100, 0);
        SetDispMask(1);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x280;
        rect.h = 0x30;
        ClearImage(&rect, 0, 0, 0);
        for (;;) {
            ClearOTagR(screen.ot, 8);
            console_printf("\n%d", cd_selected_directory + cd_file_being_read - 1);
            console_printf("\n%s", cd_get_pc_file_name(cd_file_being_read));
            console_flush(screen.ot);
            DrawOTag(&screen.ot[7]);
            DrawSync(0);
        }
    }
}

/* 80028230: Initialise disc access: the CD library (or the PC file server for other modes), the file index and directory table, reading both from sectors 24 and 40 when booting from CD. */
void cd_init_disc_access(u8 *files, u16 *directories, u32 mode) {
    cd_stat_setloc_count = 0;
    cd_stat_command_ok_count = 0;
    cd_stat_command_fail_count = 0;
    cd_stat_retry_setloc_count = 0;
    cd_stat_retry_fail_count = 0;
    cd_stat_lesmem_count = 0;
    cd_stat_error_limit_count = 0;
    cd_stat_stop_ok_count = 0;
    cd_stat_stop_fail_count = 0;
    if (mode == 0 || mode == -1) {
        while (CdInit() == 0) {
        }
        CdSetDebug(0);
        CdDataCallback(0);
        CdSyncCallback(0);
        CdReadyCallback(0);
        CdControl(7, 0, cd_command_result);
        cd_set_mode(0xA0);
        cd_sync_reads(0);
        VSync(3);
    } else {
        PCinit();
    }
    if (mode != -1) {
        cd_pc_file_names = (char *)mode;
    } else {
        cd_pc_file_names = NULL;
    }
    cd_file_index = files;
    cd_directory_table = directories;
    cd_selected_directory = 0;
    cd_pending_read_count = 0;
    cd_read_bytes_left = 0;
    cd_command_state = 0;
    cd_pc_file_descriptor = -1;
    if (mode == 0) {
        cd_read_raw_sectors(0x18, files, 0x8000, 0, 0);
        cd_sync_reads(0);
        cd_read_raw_sectors(0x28, cd_directory_table, 0x7A, 0, 0);
        cd_sync_reads(0);
    }
}

/* 800283D4: End disc access: stop the read, pause the drive (on CD) and clear the CD callbacks. */
void cd_shutdown_disc_access(void) {
    cd_stop_read(0);
    cd_sync_reads(0);
    if (cd_pc_file_names == NULL) {
        while (CdControlB(9, 0, cd_command_result) == 0) {
        }
        cd_set_mode(0xA0);
        cd_sync_reads(0);
        VSync(3);
    }
    CdDataCallback(0);
    CdSyncCallback(0);
    CdReadyCallback(0);
    cd_pending_read_count = 0;
    cd_read_bytes_left = 0;
    cd_command_state = 0;
}

/* 80028470: Select a directory by group and index in the directory table; returns it, or -1 (selecting 0) when that entry is empty. */
s32 cd_select_directory(s32 group, s32 index) {
    cd_selected_directory = cd_directory_table[group + index] - 1;
    if (cd_selected_directory < 0) {
        cd_selected_directory = 0;
        return -1;
    }
    return cd_selected_directory;
}

/* 800284B4: The directory group (a multiple of four) and index of the selected directory, both zero when none matches; returns the selection. */
s32 cd_get_selected_directory(s32 *group, s32 *index) {
    u16 *entry = cd_directory_table;
    s32 i;

    for (i = 0; i < 0x40; i++, entry++) {
        if (*entry == cd_selected_directory + 1) {
            *group = i / 4 * 4;
            *index = i - i / 4 * 4;
            break;
        }
    }
    if (i == 0x40) {
        *group = 0;
        *index = 0;
    }
    return cd_selected_directory;
}

/* 80028530: The disc number (directory table word 0x3c). */
s32 cd_get_disc_number(void) {
    return cd_directory_table[0x3C];
}

/* 80028548: The directory at group + index relative to the selected one. */
s32 cd_get_relative_directory(s32 group, s32 index) {
    return cd_directory_table[group + index] - cd_selected_directory;
}

/* 80028570: Load a whole PC file into a new heap block (four tries per file-server call); returns the block, or NULL. */
void *cd_load_pc_file(char *name, s32 *size) {
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
        block = heap_alloc(length, 0);
        read = 0;
        if (block != NULL) {
            for (i = 0; i < 4; i++) {
                read = PCread(fd, block, length);
                if (read != 0) {
                    break;
                }
            }
        }
        if (read == 0) {
            if (block != NULL) {
                heap_free(block);
            }
            block = NULL;
        }
        for (i = 0; i < 4; i++) {
            if (PCclose(fd) == 0) {
                goto done;
            }
        }
        if (block != NULL) {
            heap_free(block);
        }
    }
    block = NULL;
done:
    return block;
}

/* 800286BC: Bytes of the current read still to come. */
s32 cd_get_read_bytes_left(void) {
    return cd_read_bytes_left;
}

/* 800286CC: Nonzero while a disc read is pending, the drive is busy or a read is in progress. */
s32 cd_get_pending_read_count(void) {
    s32 state = cd_pending_read_count;

    if (state == 0) {
        if (cd_pc_file_names == NULL && CdDataSync(1) != 0) {
            return 1;
        }
        if (cd_command_state != 0) {
            return 1;
        }
    }
    return state;
}

/* 80028738: A file's byte size: from the PC file server when it knows the file, else from the file index. */
s32 cd_get_file_size(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (cd_pc_file_names != NULL) {
        fd = PCopen(cd_get_pc_file_name(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
        if (size > 0) {
            goto done;
        }
    }
    entry = &cd_file_index[(file + cd_selected_directory - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
done:
    return size;
}

/* 80028808: As 80028738 in the second directory selection (8004fe18), rounded up to words. */
s32 cd_get_aligned_file_size_in_reading_directory(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (cd_pc_file_names != NULL) {
        fd = PCopen(cd_get_pc_file_name(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
        if (size > 0) {
            return (size + 3) / 4 * 4;
        }
    }
    entry = &cd_file_index[(file + cd_reading_directory - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
    return (size + 3) / 4 * 4;
}

/* 800288EC: A file's byte size rounded up to words. */
s32 cd_get_aligned_file_size(s32 file) {
    s32 size = cd_get_file_size(file);

    return (size + 3) / 4 * 4;
}

/* 80028928: For an index entry with a negative size (a directory), its file count; 0 for a file. */
s16 cd_get_directory_file_count(s32 file) {
    u8 *entry = &cd_file_index[(file + cd_selected_directory - 1) * 7];
    s32 size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];

    if (size < 0) {
        return -size;
    }
    return 0;
}

/* 80028998: The PC file server name of a file (64 bytes per file), or NULL without the server. */
char *cd_get_pc_file_name(s32 file) {
    char *name = NULL;

    if (cd_pc_file_names != NULL) {
        name = cd_pc_file_names + (file + cd_selected_directory - 1) * 64;
    }
    return name;
}

/* 800289D0: A file's first sector in the selected directory. */
s32 cd_get_file_sector(s32 file) {
    u8 *entry = &cd_file_index[(file + cd_selected_directory - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}

/* 80028A18: A file's first sector in the second directory selection (8004fe18). */
s32 cd_get_file_sector_in_reading_directory(s32 file) {
    u8 *entry = &cd_file_index[(file + cd_reading_directory - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}

/* 80028A60: Wait for the disc (mode 0: until idle); returns the disc status. */
s32 cd_sync_reads(s32 mode) {
    if (mode == 0) {
        while (cd_get_pending_read_count() > 0) {
        }
    }
    return cd_get_pending_read_count();
}

/* 80028A94: Replace the shared ring and return the previous one. */
StreamRing *stream_select_ring(StreamRing *ring) {
    StreamRing *previous = stream_current_ring;
    stream_current_ring = ring;
    return previous;
}

/* 80028AAC: Clear every slot; the first slot's third halfword (ring offset 8) keeps
 * the low count. Returns the count, or -1 without a ring. */
s32 stream_reset_ring(void) {
    StreamRing *ring = stream_current_ring;
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

/* 80028B14: The next chunk of the stream, or NULL. With the PC file server, read the
 * next sector of the file into a free slot (opening the list's next file
 * when this one ends); from the disc, take the slot the CD callbacks filled
 * for the next sequence number. */
u8 *stream_get_next_chunk(void) {
    StreamRing *ring = stream_current_ring;
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
    if (cd_pc_file_names != NULL) {
        if (cd_pc_file_descriptor == -1) {
            return NULL;
        }
        if (cd_read_bytes_left <= 0) {
            return NULL;
        }
        for (i = 0; i < stream_slot_count; i++) {
            slot = &stream_slots[cd_read_cursor];
            index = cd_read_cursor;
            if (++cd_read_cursor >= stream_slot_count) {
                cd_read_cursor = 0;
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
            if (PCread(cd_pc_file_descriptor, payload, 0x800) != 0) {
                goto read;
            }
            cd_draw_error_indicator(i, 0, 0xFF, 0);
        }
        return NULL;
    read:
        cd_read_bytes_left -= 0x800;
        if (cd_read_bytes_left > 0) {
            return payload;
        }
        cd_read_bytes_left = 0;
        for (i = 0; i < 4; i++) {
            if (PCclose(cd_pc_file_descriptor) == 0) {
                break;
            }
            cd_draw_error_indicator(i, 0, 0, 0xFF);
        }
        cd_pc_file_descriptor = -1;
        if (cd_current_file_list != NULL) {
            request = &cd_current_file_list[++cd_read_cursor];
            file = request->file;
            cd_file_being_read = file;
            if (file > 0 && request->destination != NULL) {
                name = cd_get_pc_file_name(file);
                for (i = 0; i < 4; i++) {
                    cd_pc_file_descriptor = PCopen(name, 0, 0);
                    if (cd_pc_file_descriptor != -1) {
                        break;
                    }
                    cd_draw_error_indicator(i, 0xFF, 0, 0);
                }
                cd_read_bytes_left = cd_get_aligned_file_size_in_reading_directory(file);
                cd_pending_read_count--;
                return payload;
            }
            cd_read_bytes_left = 0;
        }
        cd_pending_read_count = 0;
        return payload;
    }
    for (i = 0; i < count; i++, slot++) {
        if (slot->state == 3 && slot->sequence == stream_next_chunk_sequence) {
            break;
        }
    }
    if (i == stream_slot_count) {
        return NULL;
    }
    stream_next_chunk_sequence++;
    return payload + (i << 11);
}

/* 80028E60: Whether any of `count` ring slots from `index` differs from `state` or runs past the last slot. */
s32 stream_has_slot_mismatch(s32 index, s32 count, s32 state) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (stream_slots[index].state != state) {
            return 1;
        }
        if (++index > stream_slot_count) {
            return 1;
        }
    }
    return 0;
}

/* 80028ECC: Merge the free run that follows slot `index` into its length. */
void stream_merge_free_slots(s32 index) {
    s16 length = stream_slots[index].length;
    s32 next = index + length;

    if (next < stream_slot_count && stream_slots[next].state == 0) {
        stream_slots[index].length = length + stream_slots[next].length;
    }
}

/* 80028F30: The next complete movie frame: its first sector header in *frame and its
 * data in *data (0 when one is available, 1 otherwise). With the PC file
 * server, first read the next sector: a frame's first sector reserves a free
 * run of slots for all its sectors, later sectors fill them. */
/* Slots are accessed as halfwords (state, sequence, free-run length): the
 * state store then may alias the frame size global, which is reloaded after
 * it (8002B8B0 does the same). */
s32 stream_get_next_movie_frame(u8 **data, StreamFrame **frame) {
    StreamRing *ring = stream_current_ring;
    StreamSlot *slots;
    u16 *slot;
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
    if (cd_pc_file_names != NULL && cd_pc_file_descriptor != -1 && cd_read_bytes_left > 0) {
        if (stream_frame_sector_index == 0) {
            for (i = 0; i < stream_slot_count; i += slot[2]) {
                slot = (u16 *)&stream_slots[i];
                if (slot[0] == 0) {
                    break;
                }
            }
            if (i >= stream_slot_count) {
                goto search;
            }
            if (cd_setmode_parameter & 8) {
                PCread(cd_pc_file_descriptor, cd_sector_buffer, 8);
                if (cd_sector_buffer[0] == 1) {
                    goto skip;
                }
            }
            offset = i << 11;
            stream_frame_headers = (u8 *)cd_read_destination + offset;
            header = (StreamFrame *)stream_frame_headers;
            PCread(cd_pc_file_descriptor, (u8 *)header, 0x20);
            stream_frame_sector_count = header->sectors;
            stream_frame_number = header->word8;
            if (slot[2] < stream_frame_sector_count) {
                if (cd_setmode_parameter & 8) {
                    PClseek(cd_pc_file_descriptor, -0x28, 1);
                } else {
                    PClseek(cd_pc_file_descriptor, -0x20, 1);
                }
                goto search;
            }
            slot[1] = stream_next_store_sequence;
            slot[0] = 3;
            rest = slot[2] - stream_frame_sector_count;
            if (rest >= 3) {
                slot[2] = stream_frame_sector_count + 1;
                slot[(stream_frame_sector_count + 1) * 4 + 2] = rest - 1;
                slot[(stream_frame_sector_count + 1) * 4] = 0;
                stream_merge_free_slots(stream_frame_sector_count + 1);
            }
            stream_next_store_sequence++;
            stream_frame_payloads = (u8 *)cd_read_destination + offset + stream_frame_sector_count * 32;
            PCread(cd_pc_file_descriptor, stream_frame_payloads, 0x7E0);
            if (cd_setmode_parameter & 8) {
                PClseek(cd_pc_file_descriptor, 0x118, 1);
            }
            cd_read_cursor = i;
            cd_read_bytes_left -= 0x800;
            stream_frame_sector_index++;
        } else {
            if (cd_setmode_parameter & 8) {
                PCread(cd_pc_file_descriptor, cd_sector_buffer, 8);
                if (cd_sector_buffer[0] == 1) {
                skip:
                    PClseek(cd_pc_file_descriptor, 0x918, 1);
                    goto search;
                }
            }
            slot = (u16 *)&stream_slots[++cd_read_cursor];
            slot[1] = stream_next_store_sequence;
            slot[0] = 3;
            stream_next_store_sequence++;
            PCread(cd_pc_file_descriptor, stream_frame_headers + stream_frame_sector_index * 32, 0x20);
            PCread(cd_pc_file_descriptor, stream_frame_payloads + stream_frame_sector_index * 0x7E0, 0x7E0);
            if (cd_setmode_parameter & 8) {
                PClseek(cd_pc_file_descriptor, 0x118, 1);
            }
            cd_read_bytes_left -= 0x800;
            if (++stream_frame_sector_index >= stream_frame_sector_count) {
                stream_frame_sector_index = 0;
            }
        }
    }
search:
    *frame = NULL;
    for (i = 0; i < count; i++, slots++) {
        if (slots->state == 3 && slots->sequence == stream_next_chunk_sequence) {
            break;
        }
    }
    if (i == stream_slot_count) {
        return 1;
    }
    header = (StreamFrame *)(payload + (i << 11));
    *frame = header;
    *data = (u8 *)header + header->sectors * 32;
    if (stream_has_slot_mismatch(i, header->sectors, 3) != 0) {
        return 1;
    }
    stream_next_chunk_sequence += header->sectors;
    return 0;
}

/* 8002945C: Release a ring chunk: clear its slot's state and return the old state (0xffff without a ring, 0 for no chunk). */
u16 stream_release_chunk(u8 *chunk) {
    StreamRing *ring = stream_current_ring;
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

/* 800294B4: Release a run of ring chunks (the chunk header's halfword 3 counts them), merge the freed run and return the first slot's old state. */
u16 stream_release_movie_frame(u8 *chunk) {
    StreamRing *ring = stream_current_ring;
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
    stream_merge_free_slots(index);
    return state;
}

/* 8002954C: Read `size` bytes from a raw disc sector (CD only); -1 with the PC file server. */
s32 cd_read_raw_sectors(s32 sector, void *destination, s32 size, s32 mode, s32 flags) {
    if (cd_pc_file_names != NULL) {
        return -1;
    }
    cd_sync_reads(0);
    cd_next_sector = sector;
    cd_read_bytes_left = size;
    return cd_start_read(0, destination, mode, flags);
}

/* 800295D8: Read a file of the selected directory; -3 for an invalid file, an empty file or no destination. */
s32 cd_read_file(s32 file, void *destination, s32 mode, s32 flags) {
    if (file <= 0 || cd_get_file_size(file) <= 0 || destination == NULL) {
        return -3;
    }
    cd_sync_reads(0);
    cd_reading_directory = cd_selected_directory;
    cd_next_sector = cd_get_file_sector(file);
    cd_read_bytes_left = cd_get_aligned_file_size(file);
    return cd_start_read(file, destination, mode, flags);
}

/* 80029690: Start reading `cd_read_bytes_left` bytes from sector cd_next_sector: into a stream ring (flags 0x100, or 0x200 with CD mode byte flags | 0xa0) or into memory; with the PC file server the file is opened (ring) or read now. */
/* The ring branch starts the read itself; its copy of the shared tail is
 * merged back by cross-jumping, but its references keep the data callback
 * address load ahead of the read-active flag store. */
s32 cd_start_read(s32 file, void *destination, s32 mode, s32 flags) {
    StreamRing *ring;
    s32 fd;
    s32 count;
    s32 i;
    CdlLOC *position;
    u8 *mode_byte;

    cd_file_being_read = file;
    for (i = 2; i >= 0; i--) {
        cd_sector_header[i] = 0;
    }
    position = &cd_setloc_parameter;
    cd_pending_read_count = 1;
    cd_read_destination = destination;
    cd_read_mode = mode & 0xFFFF;
    cd_read_cursor = 0;
    cd_current_file_list = NULL;
    cd_stop_requested = 0;
    cd_error_count = 0;
    CdIntToPos(cd_next_sector, position);
    if (flags & 0x100) {
        stream_select_ring(destination);
        ring = stream_current_ring;
        count = ring->count;
        if (count == 0) {
            return -4;
        }
        cd_read_destination = (u8 *)ring + count * 8 + 0x24;
        stream_slots = ring->slots;
        stream_slot_count = count;
        stream_next_store_sequence = 0;
        stream_next_complete_sequence = 0;
        stream_next_chunk_sequence = 0;
        stream_reset_ring();
        if (cd_pc_file_names != NULL) {
            char *name = cd_get_pc_file_name(file);
            for (i = 0; i < 4; i++) {
                cd_pc_file_descriptor = PCopen(name, 0, 0);
                if (cd_pc_file_descriptor != -1) {
                    break;
                }
                cd_draw_error_indicator(i, 0xFF, 0, 0);
            }
            return cd_pc_file_descriptor == -1 ? -3 : 0;
        }
        cd_command_state = 1;
        CdDataCallback(stream_mark_sector_complete);
        CdSyncCallback(cd_advance_command_state);
        CdReadyCallback(stream_store_sector);
        cd_stat_setloc_count++;
        CdControlF(2, (u8 *)position);
        return 0;
    } else if (flags & 0x200) {
        char *name;

        stream_select_ring(destination);
        ring = stream_current_ring;
        count = ring->count;
        if (count == 0) {
            return -4;
        }
        cd_read_destination = (u8 *)ring + count * 8 + 0x24;
        stream_slots = ring->slots;
        stream_slot_count = count;
        stream_frame_sector_index = 0;
        stream_next_store_sequence = 0;
        stream_next_complete_sequence = 0;
        stream_next_chunk_sequence = 0;
        stream_reset_ring();
        for (i = 3, mode_byte = &cd_setmode_parameter + 3; i >= 0; i--) {
            *mode_byte-- = 0;
        }
        cd_setmode_parameter = flags | 0xA0;
        if (cd_pc_file_names == NULL) {
            return 0;
        }
        name = cd_get_pc_file_name(file);
        for (i = 0; i < 4; i++) {
            cd_pc_file_descriptor = PCopen(name, 0, 0);
            if (cd_pc_file_descriptor != -1) {
                break;
            }
            cd_draw_error_indicator(i, 0xFF, 0, 0);
        }
        return cd_pc_file_descriptor == -1 ? -3 : 0;
    } else {
        if (cd_pc_file_names != NULL) {
            char *name = cd_get_pc_file_name(file);
            for (i = 0; i < 4; i++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                cd_draw_error_indicator(i, 0xFF, 0, 0);
            }
            if (fd == -1) {
                return -4;
            }
        opened:
            if (destination != NULL) {
                for (i = 0; i < 4; i++) {
                    if (PCread(fd, destination, cd_read_bytes_left) != 0) {
                        break;
                    }
                    cd_draw_error_indicator(i, 0, 0xFF, 0);
                }
            }
            for (i = 0; i < 4; i++) {
                if (PCclose(fd) == 0) {
                    cd_read_bytes_left = 0;
                    cd_pending_read_count = 0;
                    return 0;
                }
                cd_draw_error_indicator(i, 0, 0, 0xFF);
            }
            return -6;
        }
        cd_command_state = 1;
        CdDataCallback(NULL);
        CdSyncCallback(cd_advance_command_state);
        CdReadyCallback(cd_copy_file_sector);
    }
    cd_stat_setloc_count++;
    CdControlF(2, (u8 *)position);
    return 0;
}

/* 80029AFC: Read a zero-terminated file list: sort it by file, then start the CD reads (the callbacks continue them), or with the PC file server read every file now. Returns 0, or -3 for an empty list. */
s32 cd_read_file_list(FileRequest *list, s32 mode, s32 unused) {
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
    cd_sync_reads(0);
    cd_reading_directory = cd_selected_directory;
    for (i = 2; i >= 0; i--) {
        cd_sector_header[i] = 0;
    }
    cd_read_cursor = 0;
    cd_current_file_list = list;
    cd_pending_read_count = count;
    cd_remaining_list_file_count = count;
    cd_read_destination = list->destination;
    file = list->file;
    if (file == 0 || cd_read_destination == NULL) {
        cd_seek_or_pause(mode);
        cd_read_bytes_left = 0;
        cd_pending_read_count = 0;
        return 0;
    }
    cd_file_being_read = file;
    cd_next_sector = cd_get_file_sector(file);
    cd_read_bytes_left = cd_get_aligned_file_size_in_reading_directory(file);
    cd_read_mode = mode & 0xFFFF;
    cd_list_skipping_gap = 0;
    cd_stop_requested = 0;
    cd_error_count = 0;
    CdIntToPos(cd_next_sector, &cd_setloc_parameter);
    if (cd_pc_file_names != NULL) {
        for (i = 0; i < count; i++) {
            file = list[i].file;
            cd_file_being_read = file;
            name = cd_get_pc_file_name(file);
            for (j = 0; j < 4; j++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                cd_draw_error_indicator(j, 0xFF, 0, 0);
            }
            goto close;
        opened:
            if (list[i].destination != NULL) {
                for (j = 0; j < 4; j++) {
                    if (PCread(fd, list[i].destination, cd_get_aligned_file_size_in_reading_directory(file)) != 0) {
                        break;
                    }
                    cd_draw_error_indicator(j, 0, 0xFF, 0);
                }
            }
        close:
            for (j = 0; j < 4; j++) {
                if (PCclose(fd) == 0) {
                    break;
                }
                cd_draw_error_indicator(j, 0, 0, 0xFF);
            }
        }
        cd_read_bytes_left = 0;
        cd_pending_read_count = 0;
        return 0;
    }
    cd_command_state = 1;
    CdDataCallback(cd_update_pending_read_count);
    CdSyncCallback(cd_advance_command_state);
    CdReadyCallback(cd_copy_list_sector);
    cd_stat_setloc_count++;
    CdControlF(2, (u8 *)&cd_setloc_parameter);
    return 0;
}

/* 80029EB0: Start streaming a file through a ring of at least two slots with six stream parameters; with the PC file server the whole stream is pumped now. Returns 0, -3 for a bad file, -4 for a bad ring, -6 when the file does not close. */
s32 stream_start_image_load(s32 file, StreamRing *ring, s32 mode, s32 unused, u16 a, u16 b, u16 c, u16 d, u16 e, u16 f) {
    s32 count;
    char *name;
    s16 i;

    if (ring == NULL || (u32)(count = ring->count) < 2) {
        return -4;
    }
    if (file <= 0) {
        return -3;
    }
    if (cd_get_file_size(file) <= 0) {
        return -3;
    }
    cd_sync_reads(0);
    cd_reading_directory = cd_selected_directory;
    for (i = 0; i < 3; i++) {
        cd_sector_header[i] = 0;
    }
    stream_select_ring(ring);
    cd_file_being_read = file;
    cd_next_sector = cd_get_file_sector(file);
    cd_read_bytes_left = cd_get_aligned_file_size(file);
    cd_pending_read_count = 1;
    cd_read_destination = (u8 *)ring + count * 8 + 0x24;
    stream_slots = ring->slots;
    cd_read_mode = mode & 0xFFFF;
    cd_read_cursor = 0;
    stream_slot_count = count;
    stream_next_store_sequence = 0;
    stream_next_complete_sequence = 0;
    cd_current_file_list = NULL;
    cd_stop_requested = 0;
    cd_error_count = 0;
    stream_image_1200_mode = a;
    stream_image_1200_base_x = b;
    stream_image_1200_base_y = c;
    stream_image_1201_mode = d;
    stream_image_1201_base_x = e;
    stream_image_1201_base_y = f;
    stream_image_remaining_count = 0;
    stream_image_strip_x = 0;
    stream_image_strip_y = 0;
    stream_image_strip_width = 0;
    stream_image_strip_heights = 0;
    stream_image_remaining_strip_count = 0;
    stream_reset_ring();
    CdIntToPos(cd_next_sector, &cd_setloc_parameter);
    if (cd_pc_file_names != NULL) {
        name = cd_get_pc_file_name(file);
        for (i = 0; i < 4; i++) {
            stream_image_pc_file_descriptor = PCopen(name, 0, 0);
            if (stream_image_pc_file_descriptor != -1) {
                break;
            }
            cd_draw_error_indicator(i, 0xFF, 0, 0);
        }
        do {
            stream_read_pc_sector(0, 0);
            stream_load_pc_image_strip(0, 0);
        } while (cd_pending_read_count > 0);
        for (i = 0; i < 4; i++) {
            i = PCclose(stream_image_pc_file_descriptor);
            if (i == 0) {
                break;
            }
            cd_draw_error_indicator(i, 0, 0, 0xFF);
        }
        if (i != 0) {
            return -6;
        }
        cd_pending_read_count = 0;
        cd_read_bytes_left = 0;
        return 0;
    }
    cd_command_state = 1;
    CdDataCallback(stream_load_image_strip);
    CdSyncCallback(cd_advance_command_state);
    CdReadyCallback(stream_store_image_sector);
    cd_stat_setloc_count++;
    CdControlF(2, (u8 *)&cd_setloc_parameter);
    return 0;
}

/* The unit's initialized disc access and stream state (cd.h, stream.h). */
s32 cd_max_list_gap_sectors = 0x10; /* 8004FDE0 */
s32 cd_file_out_of_order_count = 0; /* 8004FDE4 */
s32 cd_list_out_of_order_count = 0; /* 8004FDE8 */
s32 cd_stream_out_of_order_count = 0; /* 8004FDEC */
u8 *cd_file_index = NULL; /* 8004FDF0 */
u16 *cd_directory_table = NULL; /* 8004FDF4 */
s32 cd_read_bytes_left = 0; /* 8004FDF8 */
s32 cd_pending_read_count = 0; /* 8004FDFC */
s32 cd_remaining_list_file_count = 0; /* 8004FE00 */
s32 cd_next_sector = 0; /* 8004FE04 */
void *cd_read_destination = NULL; /* 8004FE08 */
FileRequest *cd_current_file_list = NULL; /* 8004FE0C */
s32 cd_read_cursor = 0; /* 8004FE10 */
s32 cd_selected_directory = 0; /* 8004FE14 */
s32 cd_reading_directory = 0; /* 8004FE18 */
s32 cd_command_state = 0; /* 8004FE1C */
s32 cd_retry_reason = 0; /* 8004FE20 */
u16 stream_next_chunk_sequence = 0; /* 8004FE24 */
u16 stream_next_store_sequence = 0; /* 8004FE26 */
u16 stream_next_complete_sequence = 0; /* 8004FE28 */
StreamSlot *stream_slots = NULL; /* 8004FE2C */
StreamRing *stream_current_ring = NULL; /* 8004FE30 */
s32 cd_stop_requested = 0; /* 8004FE34 */
s32 cd_read_mode = 0; /* 8004FE38 */
s32 cd_list_skipping_gap = 0; /* 8004FE3C */
s32 stream_slot_count = 0; /* 8004FE40 */
u8 cd_movie_request_kind = 0xFF; /* 8004FE44 */
u8 cd_movie_request_index = 0; /* 8004FE45 */
u8 cd_movie_request_next_mode = 0; /* 8004FE46 */
u8 cd_movie_request_unskippable = 0; /* 8004FE47 */
char *cd_pc_file_names = NULL; /* 8004FE48 */
s32 cd_pc_file_descriptor = 0; /* 8004FE4C */

/* 8002A260: Allocate a stream ring of `count` 2,048-byte sectors (plus the slot
 * header), then select and reset it. Returns the ring or NULL. */
StreamRing *stream_create_ring(s32 count, s32 mode) {
    StreamRing *ring;

    if (count > 0) {
        ring = heap_alloc(count * 0x808 + 0x24, mode);
        if (ring == NULL) {
            return NULL;
        }
        ring->count = count;
        stream_select_ring(ring);
        stream_reset_ring();
        return ring;
    }
    return NULL;
}

/* 8002A2D0: Unless a read is already running, seek to `file` (or pause for a
 * nonpositive file) with the resident CD ready callback installed. */
void cd_seek_or_pause_if_idle(s32 file) {
    if (cd_pc_file_names == 0 && cd_get_pending_read_count() == 0) {
        cd_reading_directory = cd_selected_directory;
        if (file > 0) {
            CdIntToPos(cd_get_file_sector(file), &cd_setloc_parameter);
            cd_command_state = 3;
            CdSyncCallback(cd_advance_command_state);
            CdControlF(2, (u8 *)&cd_setloc_parameter);
        } else {
            cd_command_state = 5;
            CdSyncCallback(cd_advance_command_state);
            CdControlF(9, NULL);
        }
    }
}

/* 8002A394: Seek to `file` (or pause) unconditionally. */
void cd_seek_or_pause(s32 file) {
    if (file > 0) {
        CdIntToPos(cd_get_file_sector(file), &cd_setloc_parameter);
        cd_command_state = 3;
        CdSyncCallback(cd_advance_command_state);
        CdControlF(2, (u8 *)&cd_setloc_parameter);
    } else {
        cd_command_state = 5;
        CdSyncCallback(cd_advance_command_state);
        CdControlF(9, NULL);
    }
}

/* 8002A428: Issue CdlSetmode with `mode`, stored and passed through a pointer to the
 * parameter (8002a470: its address is formed once for the store and the
 * call, as for an array; the unit's mode byte is a scalar for 80028f30). */
void cd_set_mode(u8 mode) {
    u8 *param;
    u8 *setmode;
    s32 i;

    cd_command_state = 9;
    CdSyncCallback(cd_advance_command_state);
    for (i = 3, param = &cd_setmode_parameter + 3; i >= 0; i--) {
        *param-- = 0;
    }
    setmode = &cd_setmode_parameter;
    *setmode = mode;
    CdControlF(0xE, setmode);
}

/* 8002A498: Request a stop with `reason`; when a read is active, drop it and close the
 * open host file handle (retrying a few transient results). */
void cd_stop_read(s32 reason) {
    s32 result;

    cd_stop_requested = 1;
    cd_read_mode = reason;
    if (cd_pc_file_names != 0) {
        cd_read_bytes_left = 0;
        cd_pending_read_count = 0;
        if (cd_pc_file_descriptor != -1) {
            do {
                result = PCclose(cd_pc_file_descriptor);
            } while (result != 0 && result + 1 < 4);
            cd_pc_file_descriptor = -1;
        }
    }
}

/* 8002A524: Free every loaded file's data in a zero-terminated file table. */
void cd_free_file_table(FileEntry *table) {
    FileEntry *entry;
    void *data;

    if (table->id != 0) {
        entry = table;
        do {
            data = entry->data;
            entry++;
            if (data != NULL) {
                heap_free(data);
            }
        } while (entry->id != 0);
    }
}

/* 8002A57C: Load the files following `first` into `table` (allocated when NULL). On an
 * allocation failure everything loaded is freed and NULL returned.
 * The original recomputes &table[i] and first + i each pass, as GCC does
 * for a loop the loop optimizer does not see (here a goto loop under the
 * count guard); the terminator is written through the index. */
FileEntry *cd_alloc_directory_file_table(s32 first, FileEntry *table) {
    u8 unused[8]; /* unused in the original; reserves 8 bytes */
    s32 count;
    s32 owned = 0;
    s32 i;

    count = cd_get_directory_file_count(first);
    if (count > 0) {
        if (table == NULL) {
            table = heap_alloc((count + 1) * 8, 0);
            owned = 1;
            if (table == NULL) {
                return NULL;
            }
        }
        i = 0;
        if (count > 0) {
        next:
            table[i].id = first + i + 1;
            table[i].data = heap_alloc(cd_get_aligned_file_size(first + i + 1), 0);
            if (table[i].data == NULL) {
                cd_free_file_table(table);
                if (owned > 0) {
                    heap_free(table);
                }
                return NULL;
            }
            if (++i < count) {
                goto next;
            }
        }
        i = count;
        table[i].id = 0;
        table[i].data = NULL;
    } else {
        table = NULL;
    }
    return table;
}

/* 8002A68C: CD command-complete callback: advance the seek/read state machine
 * (cd_command_state). Status 2 is success; on failure the retry reason is kept in
 * cd_retry_reason and the drive status is polled (state 10) until it can retry. */
void cd_advance_command_state(u8 status, u8 *result) {
    switch (cd_command_state) {
    case 0:
        break;
    case 1:
        if (status == 2) {
            cd_stat_command_ok_count++;
            cd_command_state++;
            CdControlF(6, NULL);
        } else {
            cd_stat_command_fail_count++;
            cd_saved_ready_callback = CdReadyCallback(NULL);
            cd_retry_reason = 3;
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    case 2:
        if (status == 2) {
            cd_stat_command_ok_count++;
            CdSyncCallback(NULL);
            cd_command_state = 0;
        } else {
            cd_stat_command_fail_count++;
            cd_saved_ready_callback = CdReadyCallback(NULL);
            cd_retry_reason = 3;
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    case 3:
        if (status == 2) {
            cd_command_state++;
            CdControlF(0x15, NULL);
        } else {
            cd_retry_reason = 1;
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    case 4:
        if (status == 2) {
            CdSyncCallback(NULL);
            cd_command_state = 0;
        } else {
            cd_retry_reason = 1;
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    case 5:
        if (status == 2) {
            CdSyncCallback(NULL);
            cd_command_state = 0;
        } else {
            cd_retry_reason = 2;
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    case 6:
        if (status == 2) {
            cd_command_state = 1;
            cd_stat_setloc_count++;
            cd_stat_retry_setloc_count++;
            CdReadyCallback(cd_saved_ready_callback);
            CdControlF(2, (u8 *)&cd_setloc_parameter);
        } else {
            cd_retry_reason = 3;
            cd_command_state = 10;
            cd_stat_retry_fail_count++;
            CdControlF(1, NULL);
        }
        break;
    case 7:
        if (status == 2) {
            cd_command_state = 6;
            cd_stat_stop_ok_count++;
            CdControlF(9, NULL);
        } else {
            cd_retry_reason = 4;
            cd_command_state = 10;
            cd_stat_stop_fail_count++;
            CdControlF(1, NULL);
        }
        break;
    case 12:
        if (status == 2) {
            cd_command_state = 8;
            cd_setfilter_parameter.file = 1;
            cd_setfilter_parameter.chan = *(u8 *)&cd_read_mode; /* the mode's channel byte */
            CdControlF(0xD, (u8 *)&cd_setfilter_parameter);
        } else {
            cd_retry_reason = 5;
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    case 8:
        if (status == 2) {
            cd_command_state = 1;
            cd_stat_setloc_count++;
            cd_stat_retry_setloc_count++;
            CdReadyCallback(cd_saved_ready_callback);
            CdControlF(2, (u8 *)&cd_setloc_parameter);
        } else {
            cd_retry_reason = 5;
            cd_command_state = 10;
            cd_stat_retry_fail_count++;
            CdControlF(1, NULL);
        }
        break;
    case 9:
        if (status == 2) {
            cd_command_state = 5;
            CdControlF(9, NULL);
        } else {
            cd_retry_reason = 6;
            cd_command_state = 10;
            cd_stat_stop_fail_count++;
            CdControlF(1, NULL);
        }
        break;
    case 10:
        if (status == 2 && !(result[0] & 0x10)) {
            cd_command_state = 11;
            CdControlF(0x13, NULL);
        } else {
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    case 11:
        if (status == 2) {
            switch (cd_retry_reason) {
            case 1:
                cd_command_state = 3;
                CdControlF(2, (u8 *)&cd_setloc_parameter);
                break;
            case 2:
                cd_command_state = 5;
                CdControlF(9, NULL);
                break;
            case 3:
                cd_command_state = 6;
                CdControlF(9, NULL);
                break;
            case 4:
                cd_command_state = 7;
                CdControlF(8, NULL);
                break;
            case 5:
                cd_command_state = 12;
                CdControlF(0xE, &cd_setmode_parameter);
                break;
            case 6:
                cd_command_state = 9;
                CdControlF(0xE, &cd_setmode_parameter);
                break;
            }
        } else {
            cd_command_state = 10;
            CdControlF(1, NULL);
        }
        break;
    }
}

/* 8002AC24: CD data callback of list reads: copy each sector of the current file of
 * the list (cd_current_file_list) to its destination, move on to the next file (reading
 * on through a short gap, or seeking), and retry through the command state
 * machine when a sector fails or arrives out of order. */
void cd_copy_list_sector(u8 status, u8 *result) {
    FileRequest *request;
    u16 file;
    u32 first;
    s32 i;
    s32 j;

    if (status == 1) {
        if (cd_stop_requested > 0) {
            CdReadyCallback(NULL);
            CdDataCallback(NULL);
            cd_read_bytes_left = 0;
            cd_seek_or_pause(cd_read_mode);
            cd_remaining_list_file_count = 0;
            cd_pending_read_count = 0;
            return;
        }
        if (cd_read_bytes_left >= 0x800) {
            if (cd_list_skipping_gap == 0) {
                CdGetSector(cd_sector_header, 3);
                CdGetSector(cd_read_destination, 0x200);
            }
        } else if (cd_read_bytes_left > 0 && cd_list_skipping_gap == 0) {
            CdGetSector(cd_sector_header, 3);
            CdGetSector(cd_read_destination, (cd_read_bytes_left + 3) / 4);
            CdGetSector(cd_sector_buffer, 0x200 - (cd_read_bytes_left + 3) / 4);
        }
        if (CdPosToInt((CdlLOC *)cd_sector_header) != cd_next_sector && cd_list_skipping_gap == 0) {
            cd_list_out_of_order_count++;
            goto failed;
        }
        cd_read_destination = (u8 *)cd_read_destination + 0x800;
        cd_read_bytes_left -= 0x800;
        cd_next_sector++;
        if (cd_read_bytes_left > 0) {
            return;
        }
        cd_read_cursor++;
        file = cd_current_file_list[cd_read_cursor].file;
        cd_read_destination = cd_current_file_list[cd_read_cursor].destination;
        if (file != 0 && cd_read_destination != NULL) {
            first = cd_get_file_sector_in_reading_directory(file);
            cd_read_bytes_left = cd_get_aligned_file_size_in_reading_directory(file);
            if (cd_next_sector < first && cd_next_sector + cd_max_list_gap_sectors >= first) {
                /* Close ahead: read on, discarding the sectors between. */
                cd_list_skipping_gap = 1;
                cd_read_bytes_left = (cd_next_sector - first) << 11;
                cd_read_cursor--;
                return;
            }
            if (first == cd_next_sector) {
                cd_list_skipping_gap = 0;
                cd_remaining_list_file_count--;
                return;
            } else {
                cd_list_skipping_gap = 0;
                cd_next_sector = first;
                cd_saved_ready_callback = CdReadyCallback(NULL);
                CdIntToPos(cd_next_sector, &cd_setloc_parameter);
                cd_command_state = 6;
                CdSyncCallback(cd_advance_command_state);
                CdControlF(9, NULL);
            }
            cd_remaining_list_file_count--;
            return;
        }
        cd_read_bytes_left = 0;
        CdReadyCallback(NULL);
        cd_seek_or_pause(cd_read_mode);
        cd_remaining_list_file_count = 0;
        cd_pending_read_count = 0;
        return;
    }
failed:
    cd_error_count++;
    cd_saved_ready_callback = CdReadyCallback(NULL);
    CdIntToPos(cd_next_sector, &cd_setloc_parameter);
    if (cd_error_count < 3) {
        cd_retry_reason = 3;
    } else {
        for (i = 9999; i >= 0; i--) {
            for (j = 1999; j >= 0; j--) {
            }
        }
        cd_error_count = 0;
        cd_retry_reason = 4;
        cd_stat_error_limit_count++;
    }
    cd_command_state = 10;
    CdSyncCallback(cd_advance_command_state);
    CdControlF(1, NULL);
}

/* 8002B084: CD data callback of single-file reads: copy each sector to the
 * destination (the tail of a short last sector to cd_sector_buffer), finish the
 * read after the last one, and retry through the command state machine when
 * a sector fails or arrives out of order. */
void cd_copy_file_sector(u8 status, u8 *result) {
    s32 i;
    s32 j;

    if (status == 1) {
        if (cd_stop_requested <= 0) {
            if (cd_read_bytes_left >= 0x800) {
                CdGetSector(cd_sector_header, 3);
                CdGetSector(cd_read_destination, 0x200);
            } else if (cd_read_bytes_left > 0) {
                CdGetSector(cd_sector_header, 3);
                CdGetSector(cd_read_destination, (cd_read_bytes_left + 3) / 4);
                CdGetSector(cd_sector_buffer, 0x200 - (cd_read_bytes_left + 3) / 4);
            }
            if (CdPosToInt((CdlLOC *)cd_sector_header) != cd_next_sector) {
                cd_file_out_of_order_count++;
                goto failed;
            }
            cd_next_sector++;
            cd_read_destination = (u8 *)cd_read_destination + 0x800;
            cd_read_bytes_left -= 0x800;
            if (cd_read_bytes_left > 0) {
                return;
            }
        }
        CdReadyCallback(NULL);
        cd_read_bytes_left = 0;
        cd_seek_or_pause(cd_read_mode);
        cd_pending_read_count = 0;
        return;
    }
failed:
    cd_error_count++;
    cd_saved_ready_callback = CdReadyCallback(NULL);
    CdIntToPos(cd_next_sector, &cd_setloc_parameter);
    if (cd_error_count < 3) {
        cd_retry_reason = 3;
    } else {
        for (i = 9999; i >= 0; i--) {
            for (j = 1999; j >= 0; j--) {
            }
        }
        cd_error_count = 0;
        cd_retry_reason = 4;
        cd_stat_error_limit_count++;
    }
    cd_command_state = 10;
    CdSyncCallback(cd_advance_command_state);
    CdControlF(1, NULL);
}

/* 8002B2F0: CD data callback of stream reads: store each sector in the next free slot
 * of the stream ring (numbering it with stream_next_store_sequence), stop after the last one,
 * and retry through the command state machine when a sector arrives out of
 * order or no slot is free. The slot's sequence and state are written as
 * halfwords, the sequence first, which keeps the original's reload of
 * stream_next_store_sequence for the increment after the state store. */
void stream_store_sector(u8 status, u8 *result) {
    StreamSlot *slot;
    s32 index;
    s32 tried;
    s32 i;
    s32 j;

    if (status == 1) {
        if (cd_stop_requested > 0) {
            CdReadyCallback(NULL);
            CdDataCallback(NULL);
            cd_read_bytes_left = 0;
            cd_seek_or_pause(cd_read_mode);
            cd_pending_read_count = 0;
            return;
        }
        if (cd_read_bytes_left > 0) {
            for (tried = 0; tried < stream_slot_count; tried++) {
                slot = &stream_slots[cd_read_cursor];
                index = cd_read_cursor;
                cd_read_cursor++;
                if (cd_read_cursor >= stream_slot_count) {
                    cd_read_cursor = 0;
                }
                if (slot->state == 0) {
                    break;
                }
            }
            if (slot->state != 0) {
                goto retry;
            }
            CdGetSector(cd_sector_header, 3);
            if (CdPosToInt((CdlLOC *)cd_sector_header) != cd_next_sector) {
                cd_stream_out_of_order_count++;
                CdGetSector(cd_sector_buffer, 0x200);
                goto failed;
            }
            ((u16 *)slot)[1] = stream_next_store_sequence;
            ((u16 *)slot)[0] = 1;
            stream_next_store_sequence++;
            CdGetSector((u8 *)cd_read_destination + index * 0x800, 0x200);
            cd_read_bytes_left -= 0x800;
            cd_next_sector++;
            if (cd_read_bytes_left > 0) {
                return;
            }
        }
        CdReadyCallback(NULL);
        cd_read_bytes_left = 0;
        return;
    }
failed:
    cd_error_count++;
retry:
    cd_saved_ready_callback = CdReadyCallback(NULL);
    CdIntToPos(cd_next_sector, &cd_setloc_parameter);
    if (cd_error_count < 3) {
        cd_retry_reason = 3;
    } else {
        for (i = 9999; i >= 0; i--) {
            for (j = 1999; j >= 0; j--) {
            }
        }
        cd_error_count = 0;
        cd_retry_reason = 4;
        cd_stat_error_limit_count++;
    }
    cd_command_state = 10;
    CdSyncCallback(cd_advance_command_state);
    CdControlF(1, NULL);
}

/* 8002B5D0: A second, identical copy of the stream data callback 8002B2F0. */
void stream_store_image_sector(u8 status, u8 *result) {
    StreamSlot *slot;
    s32 index;
    s32 tried;
    s32 i;
    s32 j;

    if (status == 1) {
        if (cd_stop_requested > 0) {
            CdReadyCallback(NULL);
            CdDataCallback(NULL);
            cd_read_bytes_left = 0;
            cd_seek_or_pause(cd_read_mode);
            cd_pending_read_count = 0;
            return;
        }
        if (cd_read_bytes_left > 0) {
            for (tried = 0; tried < stream_slot_count; tried++) {
                slot = &stream_slots[cd_read_cursor];
                index = cd_read_cursor;
                cd_read_cursor++;
                if (cd_read_cursor >= stream_slot_count) {
                    cd_read_cursor = 0;
                }
                if (slot->state == 0) {
                    break;
                }
            }
            if (slot->state != 0) {
                goto retry;
            }
            CdGetSector(cd_sector_header, 3);
            if (CdPosToInt((CdlLOC *)cd_sector_header) != cd_next_sector) {
                cd_stream_out_of_order_count++;
                CdGetSector(cd_sector_buffer, 0x200);
                goto failed;
            }
            ((u16 *)slot)[1] = stream_next_store_sequence;
            ((u16 *)slot)[0] = 1;
            stream_next_store_sequence++;
            CdGetSector((u8 *)cd_read_destination + index * 0x800, 0x200);
            cd_read_bytes_left -= 0x800;
            cd_next_sector++;
            if (cd_read_bytes_left > 0) {
                return;
            }
        }
        CdReadyCallback(NULL);
        cd_read_bytes_left = 0;
        return;
    }
failed:
    cd_error_count++;
retry:
    cd_saved_ready_callback = CdReadyCallback(NULL);
    CdIntToPos(cd_next_sector, &cd_setloc_parameter);
    if (cd_error_count < 3) {
        cd_retry_reason = 3;
    } else {
        for (i = 9999; i >= 0; i--) {
            for (j = 1999; j >= 0; j--) {
            }
        }
        cd_error_count = 0;
        cd_retry_reason = 4;
        cd_stat_error_limit_count++;
    }
    cd_command_state = 10;
    CdSyncCallback(cd_advance_command_state);
    CdControlF(1, NULL);
}

/* 8002B8B0: Stream data step of PC file server reads, called in place of the CD
 * data callback: read a sector from the file server into the next free ring slot
 * (up to four read attempts); stop the read when no slot is free or after
 * the last sector.
 * Slots are four halfwords: state, sequence, free-run length and reserved.
 * Accessing the state and sequence as halfwords preserves the original
 * store-before-counter-read order. */
void stream_read_pc_sector(void) {
    u16 *slot;
    s32 index;
    s16 i;

    if (cd_read_bytes_left > 0) {
        for (i = 0; i < stream_slot_count; i++) {
            slot = (u16 *)&stream_slots[cd_read_cursor];
            index = cd_read_cursor;
            cd_read_cursor++;
            if (cd_read_cursor >= stream_slot_count) {
                cd_read_cursor = 0;
            }
            if (slot[0] == 0) {
                break;
            }
        }
        if (slot[0] == 0) {
            slot[0] = 1;
            slot[1] = stream_next_store_sequence;
            stream_next_store_sequence++;
            for (i = 0; i < 4; i++) {
                if (PCread(stream_image_pc_file_descriptor, (u8 *)cd_read_destination + index * 0x800, 0x800) != 0) {
                    break;
                }
                cd_draw_error_indicator(i, 0, 0xFF, 0);
            }
            cd_read_bytes_left -= 0x800;
            cd_next_sector++;
        } else {
            cd_read_bytes_left = 0;
        }
        if (cd_read_bytes_left > 0) {
            return;
        }
    }
    cd_read_bytes_left = 0;
}


/* 8002BA40: Set the pending read count (800286cc's state) to the current list's
 * file count. */
void cd_update_pending_read_count(void) {
    cd_pending_read_count = cd_remaining_list_file_count;
}

/* 8002BA58: Mark the ring slot holding the next sector in order (stream_next_complete_sequence) as
 * complete; once the read has ended, stop the data callback and seek on. */
void stream_mark_sector_complete(void) {
    StreamSlot *slot = stream_slots;
    s16 i;

    for (i = 0; i < stream_slot_count; i++, slot++) {
        if (slot->state == 1 && slot->sequence == stream_next_complete_sequence) {
            break;
        }
    }
    if (i != stream_slot_count) {
        slot->state = 3;
        stream_next_complete_sequence++;
        if (cd_read_bytes_left <= 0 && cd_pending_read_count < 2) {
            cd_read_bytes_left = 0;
            CdDataCallback(NULL);
            cd_seek_or_pause(cd_read_mode);
            cd_pending_read_count = 0;
        }
    }
}

/* 8002BB50: Image stream step: take the ring slot holding the next sector in order.
 * A sector starting an image (type 0x1200/0x1201) gives its placement,
 * width and strip heights; each following sector is one strip, loaded to
 * VRAM. After the last strip of the last image the read is stopped. */
void stream_load_image_strip(void) {
    StreamSlot *slot = stream_slots;
    s16 i;
    u32 *p;
    s32 type;
    u16 *pos;
    RECT rect;

    for (i = 0; i < stream_slot_count; i++, slot++) {
        if (slot->state == 1 && slot->sequence == stream_next_complete_sequence) {
            break;
        }
    }
    if (i == stream_slot_count) {
        return;
    }
    slot->state = 2;
    p = (u32 *)((u8 *)cd_read_destination + i * 0x800);
    if (stream_image_remaining_strip_count == 0) {
        type = *p++;
        pos = (u16 *)p;
        if (type != 0x1200 && type != 0x1201) {
            goto end;
        }
        if (type == 0x1200) {
            switch (stream_image_1200_mode) {
            case 1:
                stream_image_strip_x = stream_image_1200_base_x + pos[2];
                stream_image_strip_y = stream_image_1200_base_y + pos[3];
                break;
            case 2:
                stream_image_strip_x = stream_image_1200_base_x + pos[0] + pos[2];
                stream_image_strip_y = stream_image_1200_base_y + pos[1] + pos[3];
                break;
            default:
                stream_image_strip_x = pos[0] + pos[2];
                stream_image_strip_y = pos[1] + pos[3];
                break;
            }
        }
        if (type == 0x1201) {
            switch (stream_image_1201_mode) {
            case 1:
                stream_image_strip_x = stream_image_1201_base_x + pos[2];
                stream_image_strip_y = stream_image_1201_base_y + pos[3];
                break;
            case 2:
                stream_image_strip_x = stream_image_1201_base_x + pos[0] + pos[2];
                stream_image_strip_y = stream_image_1201_base_y + pos[1] + pos[3];
                break;
            default:
                stream_image_strip_x = pos[0] + pos[2];
                stream_image_strip_y = pos[1] + pos[3];
                break;
            }
        }
        p += 2;
        stream_image_strip_width = *(u16 *)p;
        p += 2;
        if (stream_image_remaining_count == 0) {
            stream_image_remaining_count = *p;
        }
        p++;
        stream_image_remaining_strip_count = *p++;
        stream_image_strip_heights = (u16 *)p;
    } else {
        rect.x = stream_image_strip_x;
        rect.y = stream_image_strip_y;
        rect.w = stream_image_strip_width;
        rect.h = *stream_image_strip_heights;
        LoadImage(&rect, (u_long *)p);
        stream_image_strip_y += *stream_image_strip_heights++;
        if (--stream_image_remaining_strip_count <= 0) {
            stream_image_remaining_strip_count = 0;
            stream_image_remaining_count--;
            for (i = 0; i < stream_slot_count; i++) {
                stream_slots[i].state = 0;
                stream_slots[i].sequence = 0;
            }
            if (stream_image_remaining_count <= 0) {
            end:
                cd_read_bytes_left = 0;
                CdDataCallback(NULL);
                cd_seek_or_pause(cd_read_mode);
                cd_pending_read_count = 0;
                return;
            }
        }
        slot->state = 0;
    }
    stream_next_complete_sequence++;
}

/* 8002BF38: Image stream step of PC file server reads (80029EB0): as 8002BB50, but
 * each strip load is waited for and the read simply ends. */
void stream_load_pc_image_strip(void) {
    StreamSlot *slot = stream_slots;
    s16 i;
    u32 *p;
    s32 type;
    u16 *pos;
    RECT rect;

    for (i = 0; i < stream_slot_count; i++, slot++) {
        if (slot->state == 1 && slot->sequence == stream_next_complete_sequence) {
            break;
        }
    }
    if (i == stream_slot_count) {
        return;
    }
    slot->state = 2;
    p = (u32 *)((u8 *)cd_read_destination + i * 0x800);
    if (stream_image_remaining_strip_count == 0) {
        type = *p++;
        pos = (u16 *)p;
        if (type != 0x1200 && type != 0x1201) {
            return;
        }
        if (type == 0x1200) {
            switch (stream_image_1200_mode) {
            case 1:
                stream_image_strip_x = stream_image_1200_base_x + pos[2];
                stream_image_strip_y = stream_image_1200_base_y + pos[3];
                break;
            case 2:
                stream_image_strip_x = stream_image_1200_base_x + pos[0] + pos[2];
                stream_image_strip_y = stream_image_1200_base_y + pos[1] + pos[3];
                break;
            default:
                stream_image_strip_x = pos[0] + pos[2];
                stream_image_strip_y = pos[1] + pos[3];
                break;
            }
        }
        if (type == 0x1201) {
            switch (stream_image_1201_mode) {
            case 1:
                stream_image_strip_x = stream_image_1201_base_x + pos[2];
                stream_image_strip_y = stream_image_1201_base_y + pos[3];
                break;
            case 2:
                stream_image_strip_x = stream_image_1201_base_x + pos[0] + pos[2];
                stream_image_strip_y = stream_image_1201_base_y + pos[1] + pos[3];
                break;
            default:
                stream_image_strip_x = pos[0] + pos[2];
                stream_image_strip_y = pos[1] + pos[3];
                break;
            }
        }
        p += 2;
        stream_image_strip_width = *(u16 *)p;
        p += 2;
        if (stream_image_remaining_count == 0) {
            stream_image_remaining_count = *p;
        }
        p++;
        stream_image_remaining_strip_count = *p++;
        stream_image_strip_heights = (u16 *)p;
    } else {
        rect.x = stream_image_strip_x;
        rect.y = stream_image_strip_y;
        rect.w = stream_image_strip_width;
        rect.h = *stream_image_strip_heights;
        LoadImage(&rect, (u_long *)p);
        DrawSync(0);
        stream_image_strip_y += *stream_image_strip_heights++;
        if (--stream_image_remaining_strip_count <= 0) {
            stream_image_remaining_strip_count = 0;
            stream_image_remaining_count--;
            for (i = 0; i < stream_slot_count; i++) {
                stream_slots[i].state = 0;
                stream_slots[i].sequence = 0;
            }
            if (stream_image_remaining_count <= 0) {
                cd_read_bytes_left = 0;
                cd_pending_read_count = 0;
                return;
            }
        }
        slot->state = 0;
    }
    stream_next_complete_sequence++;
}

/* 8002C310: Print the image stream state (debug report). */
void stream_print_image_state(void) {
    console_report_printf("F%8x A%8x S%8x\n", cd_file_index, cd_read_destination, stream_slots);
    console_report_printf("%d %d %d %d\n", stream_slot_count, cd_read_cursor, stream_image_remaining_count, stream_image_remaining_strip_count);
    console_report_printf("%d %d %d %8x\n", stream_image_strip_x, stream_image_strip_y, stream_image_strip_width, stream_image_strip_heights);
    console_report_printf("%d %d %d\n", stream_image_strip_heights[0], stream_image_strip_heights[1], stream_image_strip_heights[2]);
}

/* 8002C3D8: Nonzero when files come from the PC file server (its name table). */
s32 cd_has_pc_file_server(void) {
    return (s32)cd_pc_file_names;
}
