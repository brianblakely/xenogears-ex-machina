/*
 * The world map's handwritten terrain renderers
 * (src/decomp/src/worldmap/worldmap_terrain_draw_quarter_block.s,
 * worldmap_billboards_draw_block.s and their screen_bounds.s), as portable C
 * on the software GTE. Both read their work areas from the scratchpad at its
 * original address (TerrainDrawScratch and the billboard pass's scratch,
 * which the C callers fill), make the original's GTE instructions in its
 * order, so the GTE is left as the original leaves it, and write the same
 * packet words, ordering table links and counters. The originals also save
 * s registers into the caller's MIPS stack (its a0 home slot); the port has
 * no MIPS stack, so those register saves have no counterpart.
 */
#include "common.h"
#include "xem/gte_ops.h"

#define XEM_SCRATCHPAD 0x1F800000

/* Terrain POLY_FT3 packets used this frame (8009D7DC). */
extern s32 worldmap_terrain_packet_count;
/* Billboard quads used this frame (8009BE04): an s16 in the C, but the
 * original loads and stores it as a word, with the halfword after it. */
extern u32 worldmap_billboard_quad_count;
/* The camera; its target x and z (20.12) are the words at 0 and 8 (8009BE28). */
extern u8 worldmap_camera[];
/* The map's extent in blocks of 0x800 (8009D160, 8009D2B4). */
extern s32 worldmap_area_blocks_x;
extern s32 worldmap_area_blocks_z;

/* A host pointer's PS1 address. */
#define XEM_PS1_ADDRESS(pointer) ((u32)(XemUintptr)(pointer))

/* screen_bounds_test: some vertex has 0 <= x < 320 (x unsigned) and some
 * vertex, not necessarily the same one, has 0 <= y < 216 (y signed, then
 * compared unsigned). */
static s32 xem_worldmap_on_screen(u32 sxy0, u32 sxy1, u32 sxy2) {
    if ((sxy0 & 0xFFFF) >= 320 && (sxy1 & 0xFFFF) >= 320 && (sxy2 & 0xFFFF) >= 320) {
        return 0;
    }
    if ((u32)((s32)sxy0 >> 16) >= 216 && (u32)((s32)sxy1 >> 16) >= 216 && (u32)((s32)sxy2 >> 16) >= 216) {
        return 0;
    }
    return 1;
}

enum { XEM_TERRAIN_DRAWN, XEM_TERRAIN_REJECTED, XEM_TERRAIN_FLAG_ERROR };

/* Presentation only (xem/gte.h): a terrain triangle the screen-bounds test
 * culled, recorded as the POLY_FT3 it would have been (the pool's command
 * word, the page and CLUT as drawn triangles get them) when the host wants
 * culled faces, xem_present_face_visible keeps it and it is nearer than the
 * far cutoff. */
static void xem_terrain_reveal(u32 word, u32 ot, u32 packet, u32 uv0, u32 uv1, u32 uv2, u32 sxy0, u32 sxy1, u32 sxy2) {
    u32 words[7], page, clut, cue, projections = xem_gte_ring->count;
    s32 depth, sz;

    if (!xem_present_reveals->wanted || !xem_present_face_visible(projections, 3, 320, 216)) {
        return;
    }
    depth = (s32)xem_gte_read_data(XEM_GTE_SZ1);
    sz = (s32)xem_gte_read_data(XEM_GTE_SZ2);
    if (!(sz < depth)) {
        depth = sz;
    }
    sz = (s32)xem_gte_read_data(XEM_GTE_SZ3);
    if (!(sz < depth)) {
        depth = sz;
    }
    if ((u32)depth >= 0xF00) {
        return;
    }
    cue = xem_gte_read_data(XEM_GTE_IR0);
    if (cue >= 0x1000) {
        cue = 0xFFF;
    }
    page = XEM_U16(XEM_SCRATCHPAD + 0x308 + (((word << 16) >> 23) & 0xE));
    clut = XEM_U16(XEM_SCRATCHPAD + 0x288 + ((u32)((s32)cue >> 7) << 1) + (((word << 16) >> 21) & 0x40));
    words[0] = XEM_U32(packet + 4);
    words[1] = sxy0;
    words[2] = (clut << 16) | (uv0 & 0xFFFF);
    words[3] = sxy1;
    words[4] = (page << 16) | (uv1 & 0xFFFF);
    words[5] = sxy2;
    words[6] = uv2 & 0xFFFF;
    xem_present_reveal(ot + ((u32)(depth >> 4) << 2), 3, projections, words, 7);
}

/* terrain_triangle: project the three loaded vertices and, when the triangle
 * is accepted, emit the POLY_FT3 at *packet (advanced by 32) and count it.
 * `word` is the cell word, uv0..uv2 the packed u | v << 8 coordinates. */
static s32 xem_terrain_triangle(u32 word, u32 ot, u32 *packet, u32 *count, u32 uv0, u32 uv1, u32 uv2) {
    u32 sxy0, sxy1, sxy2, entry, page, clut, previous, at;
    s32 depth, sz;
    u32 cue;

    xem_gte_execute(XEM_GTE_RTPT);
    sxy0 = xem_gte_read_data(XEM_GTE_SXY0);
    if ((s32)xem_gte_read_control(31) < 0) {
        return XEM_TERRAIN_FLAG_ERROR;
    }
    sxy1 = xem_gte_read_data(XEM_GTE_SXY1);
    sxy2 = xem_gte_read_data(XEM_GTE_SXY2);
    if (!xem_worldmap_on_screen(sxy0, sxy1, sxy2)) {
        xem_terrain_reveal(word, ot, *packet, uv0, uv1, uv2, sxy0, sxy1, sxy2);
        return XEM_TERRAIN_REJECTED;
    }
    /* The largest of SZ1..SZ3, compared signed. */
    depth = (s32)xem_gte_read_data(XEM_GTE_SZ1);
    sz = (s32)xem_gte_read_data(XEM_GTE_SZ2);
    if (!(sz < depth)) {
        depth = sz;
    }
    sz = (s32)xem_gte_read_data(XEM_GTE_SZ3);
    if (!(sz < depth)) {
        depth = sz;
    }
    if ((u32)depth >= 0xF00) {
        return XEM_TERRAIN_REJECTED;
    }
    xem_gte_execute(XEM_GTE_NCLIP);
    if ((s32)xem_gte_read_data(XEM_GTE_MAC0) <= 0) {
        return XEM_TERRAIN_REJECTED;
    }
    entry = ot + ((u32)(depth >> 4) << 2);
    /* IR0, the third vertex's depth cue, limited to 0xFFF. */
    cue = xem_gte_read_data(XEM_GTE_IR0);
    if (cue >= 0x1000) {
        cue = 0xFFF;
    }
    page = XEM_U16(XEM_SCRATCHPAD + 0x308 + (((word << 16) >> 23) & 0xE));
    clut = XEM_U16(XEM_SCRATCHPAD + 0x288 + ((u32)((s32)cue >> 7) << 1) + (((word << 16) >> 21) & 0x40));
    previous = XEM_U32(entry);
    *count += 1;
    at = *packet;
    xem_store_face_sxy(at + 8, sxy0, 0, 3);
    xem_store_face_sxy(at + 16, sxy1, 1, 3);
    xem_store_face_sxy(at + 24, sxy2, 2, 3);
    XEM_U16(at + 28) = (u16)uv2;
    *packet = at + 32;
    XEM_U32(at + 12) = (clut << 16) | uv0;
    XEM_U32(at + 20) = (page << 16) | uv1;
    XEM_U32(at) = 0x07000000 | previous; /* the previous entry, not masked */
    XEM_U32(entry) = at & 0x00FFFFFF;
    return XEM_TERRAIN_DRAWN;
}

/* Load a vertex (SVECTOR at `vertex`) into V0, V1 or V2 (data register first). */
static void xem_terrain_load(s32 first, u32 vertex) {
    xem_lwc2(first, vertex);
    xem_lwc2(first + 1, vertex + 4);
}

/* Draw one terrain quarter block of 8x8 cells as POLY_FT3 pairs from the 9x9
 * vertices in the scratchpad, at `packets` onwards, linking them into `ot`.
 * The packet count is checked before each cell, so drawing stops once 0x7FE
 * packets exist (a cell may take it to 0x7FF); it is stored back on exit. */
void worldmap_terrain_draw_quarter_block(u32 *cells, u32 *ot, s32 packets) {
    u32 cell = XEM_PS1_ADDRESS(cells);
    u32 ot_address = XEM_PS1_ADDRESS(ot);
    u32 packet = (u32)packets;
    u32 vertex = XEM_SCRATCHPAD;
    u32 count = (u32)worldmap_terrain_packet_count;
    u32 word, base, tl, tr, bl, br, uv;
    s32 rows, columns;

    for (rows = 8; rows != 0; rows--) {
        for (columns = 8; columns != 0; columns--) {
            word = XEM_U32(cell);
            base = ((word >> 12) & 0xF0) | ((word >> 8) & 0xF000);
            /* The texture coordinates of TL, TR, BL and BR: bits 13 and 14
             * flip the 16x16 tile horizontally and vertically. */
            switch ((word >> 13) & 3) {
            case 0:
                tl = base, tr = base + 0xF, bl = base + 0xF00, br = base + 0xF0F;
                break;
            case 1:
                tl = base + 0xF, tr = base, bl = base + 0xF0F, br = base + 0xF00;
                break;
            case 2:
                tl = base + 0xF00, tr = base + 0xF0F, bl = base, br = base + 0xF;
                break;
            default:
                tl = base + 0xF0F, tr = base + 0xF00, bl = base + 0xF, br = base;
                break;
            }
            if (count >= 0x7FE) {
                goto done;
            }
            /* (TL, TR, BL), or with bit 15 (TL, BR, BL). */
            xem_terrain_load(XEM_GTE_VXY0, vertex);
            xem_terrain_load(XEM_GTE_VXY2, vertex + 0x48);
            if (word & 0x8000) {
                xem_terrain_load(XEM_GTE_VXY1, vertex + 0x50);
                uv = br;
            } else {
                xem_terrain_load(XEM_GTE_VXY1, vertex + 8);
                uv = tr;
            }
            /* A FLAG error on the first triangle skips the second as well. */
            if (xem_terrain_triangle(word, ot_address, &packet, &count, tl, uv, bl) != XEM_TERRAIN_FLAG_ERROR) {
                /* (TR, BR, BL), or with bit 15 (TR, BR, TL). */
                xem_terrain_load(XEM_GTE_VXY0, vertex + 8);
                xem_terrain_load(XEM_GTE_VXY1, vertex + 0x50);
                if (word & 0x8000) {
                    xem_terrain_load(XEM_GTE_VXY2, vertex);
                    uv = tl;
                } else {
                    xem_terrain_load(XEM_GTE_VXY2, vertex + 0x48);
                    uv = bl;
                }
                xem_terrain_triangle(word, ot_address, &packet, &count, tr, br, uv);
            }
            cell += 4;
            vertex += 8;
        }
        cell += 4;   /* the row's ninth cell word */
        vertex += 8; /* and its ninth vertex */
    }
done:
    worldmap_terrain_packet_count = (s32)count;
}

/* Presentation only (xem/gte.h): a quarter block the original left undrawn
 * (worldmap_terrain_draw: outside its camera's view), from its cell words
 * and its 9x9 vertices (`vertices`: the vxy, vz words the original builds
 * in the scratchpad), each triangle projected as the original projects it
 * (a FLAG error skipping the cell's second triangle too) and recorded as
 * culled where xem_terrain_reveal keeps it. `packet` holds the pool's
 * command word, `ot` is the ordering table the original links into. */
void xem_worldmap_terrain_reveal_quarter(u32 cells, u32 ot, u32 packet, const u32 *vertices) {
    const u32 *tl, *tr, *bl, *br, *a, *b, *c;
    u32 word, base, uv_tl, uv_tr, uv_bl, uv_br, uv_b, uv_c;
    s32 row, column;

    for (row = 0; row < 8; row++) {
        for (column = 0; column < 8; column++) {
            word = XEM_U32(cells + (u32)(row * 9 + column) * 4);
            base = ((word >> 12) & 0xF0) | ((word >> 8) & 0xF000);
            switch ((word >> 13) & 3) {
            case 0:
                uv_tl = base, uv_tr = base + 0xF, uv_bl = base + 0xF00, uv_br = base + 0xF0F;
                break;
            case 1:
                uv_tl = base + 0xF, uv_tr = base, uv_bl = base + 0xF0F, uv_br = base + 0xF00;
                break;
            case 2:
                uv_tl = base + 0xF00, uv_tr = base + 0xF0F, uv_bl = base, uv_br = base + 0xF;
                break;
            default:
                uv_tl = base + 0xF0F, uv_tr = base + 0xF00, uv_bl = base + 0xF, uv_br = base;
                break;
            }
            tl = vertices + (row * 9 + column) * 2;
            tr = tl + 2;
            bl = tl + 18;
            br = tl + 20;
            /* (TL, TR, BL), or with bit 15 (TL, BR, BL); then (TR, BR, BL),
             * or (TR, BR, TL). */
            b = word & 0x8000 ? br : tr;
            uv_b = word & 0x8000 ? uv_br : uv_tr;
            xem_gte_write_data(XEM_GTE_VXY0, tl[0]);
            xem_gte_write_data(XEM_GTE_VZ0, tl[1]);
            xem_gte_write_data(XEM_GTE_VXY1, b[0]);
            xem_gte_write_data(XEM_GTE_VZ1, b[1]);
            xem_gte_write_data(XEM_GTE_VXY2, bl[0]);
            xem_gte_write_data(XEM_GTE_VZ2, bl[1]);
            xem_gte_execute(XEM_GTE_RTPT);
            if ((s32)xem_gte_read_control(31) < 0) {
                continue;
            }
            xem_terrain_reveal(word, ot, packet, uv_tl, uv_b, uv_bl, xem_gte_read_data(XEM_GTE_SXY0),
                               xem_gte_read_data(XEM_GTE_SXY1), xem_gte_read_data(XEM_GTE_SXY2));
            a = tr;
            c = word & 0x8000 ? tl : bl;
            uv_c = word & 0x8000 ? uv_tl : uv_bl;
            xem_gte_write_data(XEM_GTE_VXY0, a[0]);
            xem_gte_write_data(XEM_GTE_VZ0, a[1]);
            xem_gte_write_data(XEM_GTE_VXY1, br[0]);
            xem_gte_write_data(XEM_GTE_VZ1, br[1]);
            xem_gte_write_data(XEM_GTE_VXY2, c[0]);
            xem_gte_write_data(XEM_GTE_VZ2, c[1]);
            xem_gte_execute(XEM_GTE_RTPT);
            if ((s32)xem_gte_read_control(31) < 0) {
                continue;
            }
            xem_terrain_reveal(word, ot, packet, uv_tr, uv_br, uv_c, xem_gte_read_data(XEM_GTE_SXY0),
                               xem_gte_read_data(XEM_GTE_SXY1), xem_gte_read_data(XEM_GTE_SXY2));
        }
    }
}

/* Presentation only (xem/gte.h): a billboard the screen-bounds test culled,
 * recorded as the POLY_FT4 it would have been (the slot's words, its fourth
 * corner and CLUT as a drawn billboard gets them) when the host wants
 * culled faces and xem_present_face_visible keeps it. The fourth corner
 * needs an RTPS; the GTE (and the ring's record of what its screen points
 * are) is saved and restored around it, so the game sees the GTE exactly as
 * without it. */
static void xem_billboard_reveal(u32 ot, u32 quad, u32 sxy0, u32 sxy1, u32 sxy2) {
    XemGte saved;
    u32 fifo[3], sxy[4], words[9], depth, cue, clut;
    s32 i;

    if (!xem_present_reveals->wanted) {
        return;
    }
    saved = xem_gte;
    for (i = 0; i < 3; i++) {
        fifo[i] = xem_gte_ring->fifo[i];
    }
    sxy[0] = sxy0, sxy[1] = sxy1, sxy[2] = sxy2;
    depth = xem_gte_read_data(XEM_GTE_SZ3);
    xem_lwc2(XEM_GTE_VXY0, XEM_SCRATCHPAD + 24);
    xem_lwc2(XEM_GTE_VZ0, XEM_SCRATCHPAD + 28);
    xem_gte_execute(XEM_GTE_RTPS);
    sxy[3] = xem_gte_read_data(XEM_GTE_SXY2);
    cue = xem_gte_read_data(XEM_GTE_IR0);
    xem_gte = saved;
    for (i = 0; i < 3; i++) {
        xem_gte_ring->fifo[i] = fifo[i];
    }
    if (cue >= 0x1000) {
        cue = 0xFFF;
    }
    clut = XEM_U16(XEM_SCRATCHPAD + 0x68 + ((cue >> 8) << 1));
    if (!xem_present_face_visible(xem_gte_ring->count, 4, 320, 216)) {
        return;
    }
    for (i = 0; i < 9; i++) {
        words[i] = XEM_U32(quad + 4 + i * 4);
    }
    words[1] = sxy[0];
    words[2] = (words[2] & 0xFFFF) | clut << 16;
    words[3] = sxy[1];
    words[5] = sxy[2];
    words[7] = sxy[3];
    xem_present_reveal(ot + ((u32)((s32)depth >> 4) << 2), 4, xem_gte_ring->count, words, 9);
}

/* Draw `count` (nonzero; 0 counts down from 2^32) billboards at the eight-byte
 * world positions `data` as POLY_FT4 quads from `quads` onwards, linking them
 * into `ot`. Positions are taken relative to the camera target and wrapped
 * once each way around the map; (dx, y, -dz) through the view matrix plus its
 * translation becomes the translation of the roll matrix, which projects the
 * scratchpad's corners. Nothing more is drawn once the quad count reaches
 * 512; it is stored back on exit. Leaves the roll matrix and that
 * translation in the GTE once a position is reached. */
void worldmap_billboards_draw_block(u8 *data, s32 count, u32 *ot, void *quads) {
    u32 position = XEM_PS1_ADDRESS(data);
    u32 left = (u32)count;
    u32 ot_address = XEM_PS1_ADDRESS(ot);
    u32 quad = XEM_PS1_ADDRESS(quads);
    u32 drawn = worldmap_billboard_quad_count;
    s32 camera_x = (s32)XEM_U32(XEM_PS1_ADDRESS(worldmap_camera)) >> 12;
    s32 camera_z = (s32)XEM_U32(XEM_PS1_ADDRESS(worldmap_camera) + 8) >> 12;
    s32 width = worldmap_area_blocks_x << 11;
    s32 height = worldmap_area_blocks_z << 11;
    u32 word, sxy0, sxy1, sxy2, sxy3, entry, previous, cue, depth, clut;
    s32 y, dx, dz, tx, ty, tz, i;

    for (;;) {
        if (!((s32)drawn < 512)) {
            break;
        }
        word = XEM_U32(position);
        y = (s32)word >> 16;
        dx = (s32)(s16)word - camera_x;
        dz = XEM_S16(position + 4) - camera_z;
        if (dx < -0x4000) {
            dx += width;
        }
        if (!(dx < 0x4000)) {
            dx -= width;
        }
        if (dz < -0x4000) {
            dz += height;
        }
        if (!(dz < 0x4000)) {
            dz -= height;
        }
        /* Rotate (dx, y, -dz) by the view matrix without translation, then
         * add the view translation. */
        for (i = 0; i < 5; i++) {
            xem_gte_write_control(i, XEM_U32(XEM_SCRATCHPAD + 0x28 + i * 4));
        }
        xem_gte_write_data(XEM_GTE_VXY0, ((u32)dx & 0xFFFF) | ((u32)y << 16));
        xem_gte_write_data(XEM_GTE_VZ0, (u32)-dz & 0xFFFF);
        xem_gte_execute(XEM_GTE_MVMVA(1, 0, 0, 3, 0));
        tx = (s32)xem_gte_read_data(XEM_GTE_MAC1) + (s32)XEM_U32(XEM_SCRATCHPAD + 0x3C);
        ty = (s32)xem_gte_read_data(XEM_GTE_MAC2) + (s32)XEM_U32(XEM_SCRATCHPAD + 0x40);
        tz = (s32)xem_gte_read_data(XEM_GTE_MAC3) + (s32)XEM_U32(XEM_SCRATCHPAD + 0x44);
        /* Project the corners rotated by the roll matrix about that point. */
        for (i = 0; i < 5; i++) {
            xem_gte_write_control(i, XEM_U32(XEM_SCRATCHPAD + 0x48 + i * 4));
        }
        xem_gte_write_control(5, (u32)tx);
        xem_gte_write_control(6, (u32)ty);
        xem_gte_write_control(7, (u32)tz);
        for (i = 0; i < 6; i++) {
            xem_lwc2(XEM_GTE_VXY0 + i, XEM_SCRATCHPAD + i * 4);
        }
        xem_gte_execute(XEM_GTE_RTPT);
        sxy0 = xem_gte_read_data(XEM_GTE_SXY0);
        if ((s32)xem_gte_read_control(31) >= 0) {
            sxy1 = xem_gte_read_data(XEM_GTE_SXY1);
            sxy2 = xem_gte_read_data(XEM_GTE_SXY2);
            depth = xem_gte_read_data(XEM_GTE_SZ3);
            if (!xem_worldmap_on_screen(sxy0, sxy1, sxy2) && depth < 0xE00) {
                xem_billboard_reveal(ot_address, quad, sxy0, sxy1, sxy2);
            }
            if (xem_worldmap_on_screen(sxy0, sxy1, sxy2) && depth < 0xE00) {
                xem_lwc2(XEM_GTE_VXY0, XEM_SCRATCHPAD + 24); /* corner 3 */
                xem_lwc2(XEM_GTE_VZ0, XEM_SCRATCHPAD + 28);
                entry = ot_address + ((u32)((s32)depth >> 4) << 2);
                xem_gte_execute(XEM_GTE_RTPS);
                sxy3 = xem_gte_read_data(XEM_GTE_SXY2);
                /* IR0, the fourth corner's depth cue, limited to 0xFFF. */
                cue = xem_gte_read_data(XEM_GTE_IR0);
                previous = XEM_U32(entry);
                if (cue >= 0x1000) {
                    cue = 0xFFF;
                }
                clut = XEM_U16(XEM_SCRATCHPAD + 0x68 + ((cue >> 8) << 1));
                xem_store_face_sxy(quad + 8, sxy0, 0, 4);
                xem_store_face_sxy(quad + 16, sxy1, 1, 4);
                xem_store_face_sxy(quad + 24, sxy2, 2, 4);
                xem_store_face_sxy(quad + 32, sxy3, 3, 4);
                XEM_U32(entry) = quad & 0x00FFFFFF;
                XEM_U32(quad) = previous | 0x09000000; /* the previous entry, not masked */
                XEM_U16(quad + 14) = (u16)clut;
                quad += 40;
                drawn++;
            }
        }
        position += 8;
        if (--left == 0) {
            break;
        }
    }
    worldmap_billboard_quad_count = drawn;
}
