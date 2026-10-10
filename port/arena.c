/*
 * The menu overlay's handwritten GTE helpers, copy and filter loops and
 * packet builders (the decomp/src/menu .s files other than the task switch), as
 * portable C on the software GTE. Each routine makes the original's GTE
 * transfers and commands in its order, so the GTE state it leaves is the
 * original's; memory is addressed by PS1 address (xem/memory.h), so packet
 * pointers the original reduced to 24-bit DMA addresses stay reduced where
 * the game sees them (the OT, model_current_packet).
 */
#include "common.h"
#include "xem/gte_ops.h"
#include "xem/port.h"

/* A C pointer argument as its PS1 address. */
#define XEM_ADDRESS(pointer) ((u32)(XemUintptr)(pointer))

#define XEM_DMA_ADDRESS_MASK 0x00FFFFFF

/* The resident model renderer state the mesh routines share (resident/model.h). */
extern u32 model_current_vertices;
extern u32 model_drawn_primitive_count;
extern u32 model_submitted_primitive_count;
extern u32 model_current_packet;
extern u32 model_ot;
extern u32 model_screen_y_limit;
extern u32 model_screen_x_limit;

/* The arena's (menu/stage.h, menu/node.h). */
extern u8 arena_draw_buffer_index;
extern u32 arena_stage_ground_triangles[2];
extern u32 arena_stage_height_map;
extern s32 arena_mesh_light_direction[3];

/* ---------------------------------------------------------------- GTE helpers */

/* vector_scale.s: three signed halfwords `stride` bytes apart into IR1-IR3,
 * the scale into IR0, GPF with shift `sf`, the saturated IR1-IR3 to the
 * first three halfwords of `out` (its fourth is untouched). */
static void xem_scale_vector(u32 in, u32 out, s32 scale, u32 stride, u32 sf) {
    s32 x = XEM_S16(in);
    s32 y = XEM_S16(in + stride);
    s32 z = XEM_S16(in + 2 * stride);

    xem_gte_write_data(XEM_GTE_IR0, scale);
    xem_gte_write_data(XEM_GTE_IR1, x);
    xem_gte_write_data(XEM_GTE_IR2, y);
    xem_gte_write_data(XEM_GTE_IR3, z);
    xem_gte_execute(XEM_GTE_GPF(sf));
    x = xem_gte_read_data(XEM_GTE_IR1);
    y = xem_gte_read_data(XEM_GTE_IR2);
    z = xem_gte_read_data(XEM_GTE_IR3);
    XEM_U16(out) = x;
    XEM_U16(out + 2) = y;
    XEM_U16(out + 4) = z;
}

/* Scale an SVECTOR by a 4.12 scale (GPF, sf=1). */
void arena_gte_scale_svector(void *dir, void *out, s32 scale) {
    xem_scale_vector(XEM_ADDRESS(dir), XEM_ADDRESS(out), scale, 2, 1);
}

/* Multiply an SVECTOR by an integer (GPF, sf=0). */
void arena_gte_multiply_svector(void *dir, void *out, s32 scale) {
    xem_scale_vector(XEM_ADDRESS(dir), XEM_ADDRESS(out), scale, 2, 0);
}

/* Scale the signed low halfwords of a VECTOR's three words (sf=1); their
 * upper halves are ignored. */
void arena_gte_scale_vector_low_halves(void *dir, void *out, s32 scale) {
    xem_scale_vector(XEM_ADDRESS(dir), XEM_ADDRESS(out), scale, 4, 1);
}

/* Rotate an SVECTOR by the GTE rotation matrix without translation
 * (MVMVA sf=1, cv=3), feed MAC1-MAC3 back through IR1-IR3 and scale them
 * by IR0 = scale (GPF sf=1); the saturated IR1-IR3 to `out`. */
void arena_gte_rotate_scale_svector(void *vector, void *out, s32 scale) {
    u32 in = XEM_ADDRESS(vector);
    u32 to = XEM_ADDRESS(out);
    u32 x, y, z;

    xem_lwc2(XEM_GTE_VXY0, in);
    xem_lwc2(XEM_GTE_VZ0, in + 4);
    xem_gte_write_data(XEM_GTE_IR0, scale);
    xem_gte_execute(XEM_GTE_MVMVA(1, 0, 0, 3, 0));
    x = xem_gte_read_data(XEM_GTE_MAC1);
    y = xem_gte_read_data(XEM_GTE_MAC2);
    z = xem_gte_read_data(XEM_GTE_MAC3);
    xem_gte_write_data(XEM_GTE_IR1, x);
    xem_gte_write_data(XEM_GTE_IR2, y);
    xem_gte_write_data(XEM_GTE_IR3, z);
    xem_gte_execute(XEM_GTE_GPF(1));
    XEM_U16(to) = xem_gte_read_data(XEM_GTE_IR1);
    XEM_U16(to + 2) = xem_gte_read_data(XEM_GTE_IR2);
    XEM_U16(to + 4) = xem_gte_read_data(XEM_GTE_IR3);
}

/* Scale the columns of MATRIX *m by the halfword scales at `scale` (4.12):
 * the matrix becomes the GTE rotation (unscaled, as it stays), then each
 * column is R * (one scale on its axis) through MVMVA sf=1 on IR, saturated.
 * Matrix and scales are read before the first store; padding and
 * translation are untouched. */
void arena_gte_scale_matrix_columns(void *m, s16 *scale) {
    u32 matrix = XEM_ADDRESS(m);
    u32 scales = XEM_ADDRESS(scale);
    u32 sx, sy, sz;
    u32 c0, c1, c2;

    xem_gte_write_control(0, XEM_U32(matrix));
    xem_gte_write_control(1, XEM_U32(matrix + 4));
    xem_gte_write_control(2, XEM_U32(matrix + 8));
    xem_gte_write_control(3, XEM_U32(matrix + 12));
    sx = XEM_U16(scales);
    xem_gte_write_control(4, XEM_U32(matrix + 16));
    sy = XEM_U16(scales + 2);
    sz = XEM_U16(scales + 4);

    xem_gte_write_data(XEM_GTE_IR1, sx);
    xem_gte_write_data(XEM_GTE_IR2, 0);
    xem_gte_write_data(XEM_GTE_IR3, 0);
    xem_gte_execute(XEM_GTE_MVMVA(1, 0, 3, 3, 0));
    c0 = xem_gte_read_data(XEM_GTE_IR1);
    c1 = xem_gte_read_data(XEM_GTE_IR2);
    c2 = xem_gte_read_data(XEM_GTE_IR3);

    xem_gte_write_data(XEM_GTE_IR1, 0);
    xem_gte_write_data(XEM_GTE_IR2, sy);
    xem_gte_write_data(XEM_GTE_IR3, 0);
    XEM_U16(matrix) = c0;
    xem_gte_execute(XEM_GTE_MVMVA(1, 0, 3, 3, 0));
    XEM_U16(matrix + 6) = c1;
    XEM_U16(matrix + 12) = c2;
    c0 = xem_gte_read_data(XEM_GTE_IR1);
    c1 = xem_gte_read_data(XEM_GTE_IR2);
    c2 = xem_gte_read_data(XEM_GTE_IR3);

    xem_gte_write_data(XEM_GTE_IR1, 0);
    xem_gte_write_data(XEM_GTE_IR2, 0);
    xem_gte_write_data(XEM_GTE_IR3, sz);
    XEM_U16(matrix + 2) = c0;
    XEM_U16(matrix + 8) = c1;
    xem_gte_execute(XEM_GTE_MVMVA(1, 0, 3, 3, 0));
    XEM_U16(matrix + 14) = c2;
    XEM_U16(matrix + 4) = xem_gte_read_data(XEM_GTE_IR1);
    XEM_U16(matrix + 10) = xem_gte_read_data(XEM_GTE_IR2);
    XEM_U16(matrix + 16) = xem_gte_read_data(XEM_GTE_IR3);
}

/* ---------------------------------------------------------------- copy and filter */

/* Box-filter RGB555 pixels in place: each pixel from `pixels` up to the one
 * before `end` becomes the quarter sum (IRGB unpacks, ORGB packs) of
 * itself, its right neighbour and the two pixels 0x280 bytes below; bit 15
 * is dropped. No width or empty-range guard: the loop stops only when the
 * read cursor meets `end`, and it reads the pair at `end` too. */
void arena_box_filter_rgb555(void *pixels, void *end) {
    u32 at = XEM_ADDRESS(pixels);
    u32 stop = XEM_ADDRESS(end);
    u32 upper, lower;
    s32 ur, ug, ub; /* upper left colour, IR1-IR3 of IRGB */
    s32 lr, lg, lb; /* lower left colour */
    s32 r, g, b;

    upper = XEM_U16(at);
    lower = XEM_U16(at + 0x280);
    xem_gte_write_data(XEM_GTE_IRGB, upper);
    ur = xem_gte_read_data(XEM_GTE_IR1);
    ug = xem_gte_read_data(XEM_GTE_IR2);
    ub = xem_gte_read_data(XEM_GTE_IR3);
    xem_gte_write_data(XEM_GTE_IRGB, lower);
    lr = xem_gte_read_data(XEM_GTE_IR1);
    lg = xem_gte_read_data(XEM_GTE_IR2);
    lb = xem_gte_read_data(XEM_GTE_IR3);
    at += 2;
    upper = XEM_U16(at);
    lower = XEM_U16(at + 0x280);
    do {
        xem_gte_write_data(XEM_GTE_IRGB, upper);
        r = ur;
        g = ug;
        b = ub;
        ur = xem_gte_read_data(XEM_GTE_IR1);
        ug = xem_gte_read_data(XEM_GTE_IR2);
        ub = xem_gte_read_data(XEM_GTE_IR3);
        r += ur;
        g += ug;
        b += ub;
        xem_gte_write_data(XEM_GTE_IRGB, lower);
        r += lr;
        g += lg;
        b += lb;
        lr = xem_gte_read_data(XEM_GTE_IR1);
        lg = xem_gte_read_data(XEM_GTE_IR2);
        lb = xem_gte_read_data(XEM_GTE_IR3);
        r += lr;
        g += lg;
        b += lb;
        xem_gte_write_data(XEM_GTE_IR1, r >> 2);
        xem_gte_write_data(XEM_GTE_IR2, g >> 2);
        xem_gte_write_data(XEM_GTE_IR3, b >> 2);
        at += 2;
        upper = XEM_U16(at);
        lower = XEM_U16(at + 0x280);
        XEM_U16(at - 4) = xem_gte_read_data(XEM_GTE_ORGB);
    } while (at != stop);
}

/* Copy `size` bytes forward a word at a time. The end test follows each
 * copy: a size of 0 (or one not a multiple of 4) never meets the end, as in
 * the original; callers pass positive word multiples. */
void arena_copy_words(void *dst, void *src, s32 size) {
    u32 to = XEM_ADDRESS(dst);
    u32 from = XEM_ADDRESS(src);
    u32 end = from + size;
    u32 word;

    do {
        word = XEM_U32(from);
        from += 4;
        XEM_U32(to) = word;
        to += 4;
    } while (from != end);
}

/* ---------------------------------------------------------------- ground */

/* The scratchpad layout arena_stage_draw_ground_cells reads and writes. */
#define XEM_GROUND_RIGHT_LIMITS 0x1F800000 /* per row: exclusive right column */
#define XEM_GROUND_LEFT_LIMITS 0x1F800080  /* per row: left column, 0xFF empty */
#define XEM_GROUND_CORNERS 0x1F800100      /* four SVECTOR corners */
#define XEM_GROUND_MAP_TABLE 0x1F800120    /* MapTable: 4 UV orientations, then 4 tpage/CLUT words */

/* ground_packet.s: emit one POLY_FT3 at `*packet` for the projected and
 * accepted triangle whose UVs are halfwords 0, `second` and `third` (byte
 * offsets) of the orientation `uvs`: depth-cue RGBC (DPCS), UVs with the UV
 * high nibbles, CLUT and texture page, the depth-cued RGB2 word with its
 * command byte, the SXY words, and a link at OT[max(SZ1, SZ2, SZ3) >> 4].
 * The packet pointer becomes its DMA address before the SXY stores and
 * stays so. */
static void xem_ground_packet(u32 ot, u32 *packet, u32 nibbles, u32 uvs, u32 tpage_clut, u32 second, u32 third) {
    u32 slot;
    s32 depth, sz2, sz3;
    u32 link;

    xem_gte_execute(XEM_GTE_DPCS);
    XEM_U32(*packet + 12) = nibbles | XEM_U16(uvs) | (tpage_clut & 0xFFFF0000);
    XEM_U32(*packet + 20) = nibbles | XEM_U16(uvs + second) | (tpage_clut << 16);
    XEM_U16(*packet + 28) = nibbles | XEM_U16(uvs + third);
    depth = xem_gte_read_data(XEM_GTE_SZ1);
    sz2 = xem_gte_read_data(XEM_GTE_SZ2);
    sz3 = xem_gte_read_data(XEM_GTE_SZ3);
    xem_swc2(XEM_GTE_RGB2, *packet + 4);
    if (!(sz2 < depth)) {
        depth = sz2;
    }
    *packet &= XEM_DMA_ADDRESS_MASK;
    if (!(sz3 < depth)) {
        depth = sz3;
    }
    slot = ot + (((u32)depth >> 4) << 2);
    link = XEM_U32(slot);
    xem_swc2(XEM_GTE_SXY2, *packet + 24);
    xem_swc2(XEM_GTE_SXY0, *packet + 8);
    xem_swc2(XEM_GTE_SXY1, *packet + 16);
    XEM_U32(slot) = *packet;
    XEM_U32(*packet) = link | 0x07000000;
    *packet += 32;
}

/* Draw the height-map cells of rows 0-126 that the scratchpad's row limits
 * select, each as two textured triangles from the draw buffer's pool
 * (arena_stage_ground_triangles). Rotation, translation, depth cue and
 * RGBC are in the GTE. The four corners go through the scratchpad; each
 * accepted span adds its cell count to model_submitted_primitive_count and
 * the emitted triangles are added to model_drawn_primitive_count and
 * returned. No OT bounds or GTE flag test is made. */
u32 arena_stage_draw_ground_cells(u32 *ot, s32 originX, s32 originZ) {
    u32 table = XEM_ADDRESS(ot);
    u32 corners = XEM_GROUND_CORNERS;
    u32 packet = arena_stage_ground_triangles[arena_draw_buffer_index];
    u32 base;
    u32 row, left, right, cells;
    u32 record, row_end;
    s32 x, z;
    u32 heights, flags, nibbles, uvs, tpage_clut;
    u32 emitted;

    for (row = 0; row < 0x7F; row++) {
        right = XEM_U8(XEM_GROUND_RIGHT_LIMITS + row);
        if (right == 0) {
            continue;
        }
        left = XEM_U8(XEM_GROUND_LEFT_LIMITS + row);
        if (left == 0xFF || !(left < right)) {
            continue;
        }
        cells = right - left;
        z = (row << 8) - originZ;
        x = (left << 8) - originX;
        model_submitted_primitive_count += cells;
        record = arena_stage_height_map + (((row << 7) + left) << 2);
        row_end = record + (cells << 2);
        do {
            /* Corners: upper left, lower right, upper right, lower left. */
            XEM_U16(corners + 24) = x;
            XEM_U16(corners + 0) = x;
            XEM_U16(corners + 20) = z;
            XEM_U16(corners + 4) = z;
            x += 0x100;
            XEM_U16(corners + 16) = x;
            XEM_U16(corners + 8) = x;
            XEM_U16(corners + 28) = z + 0x100;
            XEM_U16(corners + 12) = z + 0x100;
            heights = XEM_U32(record);
            XEM_U16(corners + 2) = heights;
            XEM_U16(corners + 10) = XEM_U16(record + 0x204);
            XEM_U16(corners + 18) = XEM_U16(record + 4);
            XEM_U16(corners + 26) = XEM_U16(record + 0x200);
            xem_lwc2(XEM_GTE_VXY0, corners + 0);
            xem_lwc2(XEM_GTE_VZ0, corners + 4);
            xem_lwc2(XEM_GTE_VXY1, corners + 8);
            xem_lwc2(XEM_GTE_VZ1, corners + 12);
            xem_lwc2(XEM_GTE_VXY2, corners + 16);
            xem_lwc2(XEM_GTE_VZ2, corners + 20);
            flags = heights >> 16;
            nibbles = flags & 0xF0F0;
            xem_gte_execute(XEM_GTE_RTPT);
            uvs = XEM_GROUND_MAP_TABLE + ((flags & 3) << 3);
            tpage_clut = XEM_U32(XEM_GROUND_MAP_TABLE + (flags & 12) + 32);
            xem_gte_execute(XEM_GTE_NCLIP);
            /* The second triangle: upper left (still in V0), lower left,
             * lower right. */
            xem_lwc2(XEM_GTE_VXY1, corners + 24);
            xem_lwc2(XEM_GTE_VZ1, corners + 28);
            xem_lwc2(XEM_GTE_VXY2, corners + 8);
            xem_lwc2(XEM_GTE_VZ2, corners + 12);
            if ((s32)xem_gte_read_data(XEM_GTE_MAC0) > 0) {
                xem_ground_packet(table, &packet, nibbles, uvs, tpage_clut, 6, 2);
            }
            xem_gte_execute(XEM_GTE_RTPT);
            xem_gte_execute(XEM_GTE_NCLIP);
            record += 4;
            if ((s32)xem_gte_read_data(XEM_GTE_MAC0) > 0) {
                xem_ground_packet(table, &packet, nibbles, uvs, tpage_clut, 4, 6);
            }
        } while (record != row_end);
    }
    base = arena_stage_ground_triangles[arena_draw_buffer_index] & XEM_DMA_ADDRESS_MASK;
    emitted = ((packet & XEM_DMA_ADDRESS_MASK) - base) >> 5;
    model_drawn_primitive_count += emitted;
    return emitted;
}

/* ---------------------------------------------------------------- meshes */

/* mesh_packet.s mesh_face_vectors: the record's vertex addresses (index 1's
 * byte offset keeps its low thirteen bits) and V1, V2 from them; V0 too
 * unless the caller defers it (returned in *first). */
static void xem_mesh_face_vectors(u32 record, u32 vertices, u32 *first, s32 defer_first) {
    u32 word = XEM_U32(record);
    u32 third = XEM_U16(record + 4);
    u32 vertex = ((word & 0xFFFF) << 3) + vertices;

    if (defer_first) {
        *first = vertex;
    } else {
        xem_lwc2(XEM_GTE_VXY0, vertex);
        xem_lwc2(XEM_GTE_VZ0, vertex + 4);
    }
    vertex = ((word >> 13) & 0xFFF8) + vertices;
    xem_lwc2(XEM_GTE_VXY1, vertex);
    xem_lwc2(XEM_GTE_VZ1, vertex + 4);
    vertex = (third << 3) + vertices;
    xem_lwc2(XEM_GTE_VXY2, vertex);
    xem_lwc2(XEM_GTE_VZ2, vertex + 4);
}

/* mesh_packet_link: the SXY words, then the packet prepended at the one OT
 * slot model_ot (the old word ORed with the length, not masked again). */
static void xem_mesh_link(u32 packet, u32 slot, u32 length, const u32 *sxy, s32 vertices) {
    s32 i;
    u32 link;

    for (i = 0; i < vertices; i++) {
        XEM_U32(packet + 8 + i * 4) = sxy[i];
    }
    link = XEM_U32(slot);
    XEM_U32(slot) = packet;
    XEM_U32(packet) = link | length;
}

/* Flat triangles into consecutive 20-byte POLY_F3 slots from
 * model_current_packet (as its DMA address), prepended to model_ot's one
 * slot. RTPT is pipelined: the record after the last is read and projected
 * too, also for a count of 0. A face is culled unless some SXY passes the
 * unsigned y test; it is then counted, and linked if NCLIP is positive and
 * some x passes the unsigned x test. Slots advance for culled faces too. */
void arena_mesh_draw_flat_triangles(u8 *prims, s32 count) {
    u32 record = XEM_ADDRESS(prims);
    u32 left = count;
    u32 vertices = model_current_vertices;
    u32 drawn = model_drawn_primitive_count;
    u32 packet = model_current_packet;
    u32 slot = model_ot;
    u32 y_limit = model_screen_y_limit;
    u32 x_limit = model_screen_x_limit;
    u32 sxy[3];

    xem_mesh_face_vectors(record, vertices, NULL, 0);
    packet = (packet - 20) & XEM_DMA_ADDRESS_MASK;
    for (;;) {
        xem_gte_execute(XEM_GTE_RTPT);
        if (left-- == 0) {
            break;
        }
        record += 8;
        packet += 20;
        xem_mesh_face_vectors(record, vertices, NULL, 0);
        sxy[0] = xem_gte_read_data(XEM_GTE_SXY0);
        sxy[1] = xem_gte_read_data(XEM_GTE_SXY1);
        sxy[2] = xem_gte_read_data(XEM_GTE_SXY2);
        xem_gte_execute(XEM_GTE_NCLIP);
        if (!(sxy[0] < y_limit || sxy[1] < y_limit || sxy[2] < y_limit)) {
            continue;
        }
        drawn++;
        if ((s32)xem_gte_read_data(XEM_GTE_MAC0) <= 0) {
            continue;
        }
        if (!((sxy[0] & 0xFFFF) < x_limit || (sxy[1] & 0xFFFF) < x_limit || (sxy[2] & 0xFFFF) < x_limit)) {
            continue;
        }
        xem_mesh_link(packet, slot, 4 << 24, sxy, 3);
    }
    model_drawn_primitive_count = drawn;
    model_current_packet = packet + 20;
}

/* Flat quads into consecutive 24-byte POLY_F4 slots, as the triangles:
 * RTPT projects the first three points and NCLIP tests their winding; every
 * face is counted; a front-facing quad's fourth point is projected by RTPS
 * (its VXY0 is loaded for a back face too), and the quad is linked when
 * some of its four SXY passes the y test and some the x test. */
void arena_mesh_draw_flat_quads(u8 *prims, s32 count) {
    u32 record = XEM_ADDRESS(prims);
    u32 left = count;
    u32 vertices = model_current_vertices;
    u32 drawn = model_drawn_primitive_count;
    u32 packet = model_current_packet;
    u32 slot = model_ot;
    u32 y_limit = model_screen_y_limit;
    u32 x_limit = model_screen_x_limit;
    u32 first, fourth;
    u32 sxy[4];

    xem_mesh_face_vectors(record, vertices, &first, 1);
    packet = (packet - 24) & XEM_DMA_ADDRESS_MASK;
    for (;;) {
        xem_lwc2(XEM_GTE_VXY0, first);
        xem_lwc2(XEM_GTE_VZ0, first + 4);
        xem_gte_execute(XEM_GTE_RTPT);
        if (left-- == 0) {
            break;
        }
        record += 8;
        packet += 24;
        xem_mesh_face_vectors(record, vertices, &first, 1);
        sxy[0] = xem_gte_read_data(XEM_GTE_SXY0);
        sxy[1] = xem_gte_read_data(XEM_GTE_SXY1);
        sxy[2] = xem_gte_read_data(XEM_GTE_SXY2);
        xem_gte_execute(XEM_GTE_NCLIP);
        /* Index 3 is the last halfword of the record before `record`. */
        fourth = (XEM_U16(record - 2) << 3) + vertices;
        drawn++;
        xem_lwc2(XEM_GTE_VXY0, fourth);
        if ((s32)xem_gte_read_data(XEM_GTE_MAC0) <= 0) {
            continue;
        }
        xem_lwc2(XEM_GTE_VZ0, fourth + 4);
        xem_gte_execute(XEM_GTE_RTPS);
        sxy[3] = xem_gte_read_data(XEM_GTE_SXY2);
        if (!(sxy[0] < y_limit || sxy[1] < y_limit || sxy[2] < y_limit || sxy[3] < y_limit)) {
            continue;
        }
        if (!((sxy[0] & 0xFFFF) < x_limit || (sxy[1] & 0xFFFF) < x_limit || (sxy[2] & 0xFFFF) < x_limit ||
              (sxy[3] & 0xFFFF) < x_limit)) {
            continue;
        }
        xem_mesh_link(packet, slot, 5 << 24, sxy, 4);
    }
    model_drawn_primitive_count = drawn;
    model_current_packet = packet + 24;
}

/* The R3000's signed division by a nonzero denominator with the overflow
 * trap (break 6) of the original's expanded div (its divide-by-zero trap is
 * unreachable: the caller skips a zero denominator). The trap is reported
 * to the host, which records it and continues; the quotient register then
 * holds what the R3000 leaves, 0x80000000. */
static s32 xem_mesh_shadow_divide(s32 numerator, s32 denominator) {
    if (denominator == -1 && (u32)numerator == 0x80000000) {
        xem_host_debug_break(6);
        return numerator;
    }
    return numerator / denominator;
}

/* Project the mesh's vertices, transformed by the GTE rotation and
 * translation (MVMVA sf=1, V0, TR), along arena_mesh_light_direction L onto
 * y = 0: work.vx = (x*L.y - L.x*y) / (L.y - y), work.vz = (z*L.y - L.z*y) /
 * (L.y - y), products and sums wrapping to 32 bits, stores truncated to 16.
 * A zero denominator leaves the work record intact; vy and the pad are never
 * written. The input after the last is read into V0. count must be positive
 * (0 runs 2^32 vertices, as in the original). */
void arena_mesh_project_shadow(void *vertices, u8 *work, s32 count) {
    u32 in = XEM_ADDRESS(vertices);
    u32 out = XEM_ADDRESS(work);
    u32 left = count;
    u32 lx = arena_mesh_light_direction[0];
    u32 ly = arena_mesh_light_direction[1];
    u32 lz = arena_mesh_light_direction[2];
    u32 x, y, z, denominator;

    xem_lwc2(XEM_GTE_VXY0, in);
    xem_lwc2(XEM_GTE_VZ0, in + 4);
    in += 8;
    do {
        xem_gte_execute(XEM_GTE_MVMVA(1, 0, 0, 0, 0));
        xem_lwc2(XEM_GTE_VXY0, in);
        xem_lwc2(XEM_GTE_VZ0, in + 4);
        y = xem_gte_read_data(XEM_GTE_MAC2);
        x = xem_gte_read_data(XEM_GTE_MAC1);
        denominator = ly - y;
        if (denominator != 0) {
            z = xem_gte_read_data(XEM_GTE_MAC3);
            XEM_U16(out) = xem_mesh_shadow_divide(x * ly - lx * y, denominator);
            XEM_U16(out + 4) = xem_mesh_shadow_divide(-(-z * ly + lz * y), denominator);
        }
        out += 8;
        in += 8;
    } while (--left != 0);
}
