/*
 * The resident model primitive renderers and the environment-map patcher
 * (decomp/src/resident/model_draw*.s, model_depth.s,
 * model_set_envmap_mapping.s), as portable C on the software GTE.
 */
#include "common.h"
#include "xem/gte_ops.h"

/*
 * The renderers draw(records, count) of model_primitive_types. Each walks
 * `count` eight-byte face records (u16 vertex indices 0-2, and 3 for quads)
 * and owns one packet slot per face from model_current_packet on; it writes
 * the screen coordinates (and some colours) of the faces it keeps and links
 * them into model_ot. Rotation, translation, projection, depth cueing and
 * lights are already in the GTE.
 *
 * The ports keep the originals' order of GTE work, so the GTE is left as
 * the original leaves it: each loop projects the face it is about to test
 * and loads the next record's vertices while the GTE works, so the record
 * after the last face is read and projected too (a count of 0 still
 * projects the first face), and the commands issued in branch delay slots
 * are issued on the same paths. The error test after RTPT/RTPS reads LZCR
 * (data register 31), not FLAG; LZCR never has bit 31 set, so faces whose
 * projection overflowed are never culled (docs/later-phases.md, Original
 * quirks). On the culling path the slot pointer is reduced to its 24-bit
 * DMA address (main RAM is mirrored at 0) and stays so, also in
 * model_current_packet; accesses go through xem_address. A linked packet's
 * tag is the old OT word ORed with the tag length, without masking.
 */

extern u32 model_current_vertices;      /* SVECTOR *: the vertex array */
extern u32 model_current_normals;       /* SVECTOR *: the vertex normals */
extern s32 model_drawn_primitive_count; /* primitives drawn */
extern u32 model_current_packet;        /* the next packet slot */
extern u32 model_ot;                    /* u32 *: the ordering table */
extern u32 model_screen_y_limit;        /* the y bound << 16 */
extern u32 model_screen_x_limit;        /* the x bound */
extern s32 model_ot_depth_shift;        /* OT index = depth >> shift */
extern u32 model_lit_color_cache;       /* s32 *: the lit-colour cache */
extern u32 model_color;                 /* CVECTOR: the depth cue colour word */

#define XEM_DMA_MASK 0x00FFFFFF

/* The scratchpad words the vertex-normal renderers spill a face's normal
 * addresses to. */
#define XEM_SPILL 0x1F800000

/* A renderer's registers: a0, a1, s0, s2, s3, s4, v0, v1, the OT depth
 * shift and the record being decoded (t4, t5). */
typedef struct {
    u32 record;
    s32 left;
    u32 vertices;
    s32 drawn;
    u32 slot;
    u32 ot;
    u32 y_limit;
    u32 x_limit;
    s32 shift;
    u32 word;
    u32 half;
} XemDraw;

/* Load the drawing state (model_draw_state). */
static void xem_draw_begin(XemDraw *d, u8 *records, s32 count) {
    d->record = (u32)(XemUintptr)records;
    d->left = count;
    d->vertices = model_current_vertices;
    d->drawn = model_drawn_primitive_count;
    d->slot = model_current_packet;
    d->ot = model_ot;
    d->y_limit = model_screen_y_limit;
    d->x_limit = model_screen_x_limit;
    d->shift = model_ot_depth_shift;
}

/* The shared exit (model_draw_gt3_avg.s, 8002E1F4): step past the last
 * face's slot and store the slot pointer and counter. */
static void xem_draw_end(XemDraw *d, u32 size) {
    d->slot += size;
    model_drawn_primitive_count = d->drawn;
    model_current_packet = d->slot;
}

static void xem_draw_read(XemDraw *d) {
    d->word = XEM_U32(d->record);
    d->half = XEM_U16(d->record + 4);
}

/* Step to the next record (a0 += 8 and its loads). */
static void xem_draw_next(XemDraw *d) {
    d->record += 8;
    xem_draw_read(d);
}

/* Vertex addresses (model_vertex0-2): index 0 whole, or masked to thirteen
 * bits as some loops do; index 1 always to thirteen bits; index 3 is the
 * halfword before the next record. */
static u32 xem_vertex0(XemDraw *d) {
    return ((d->word & 0xFFFF) << 3) + d->vertices;
}

static u32 xem_vertex0_13(XemDraw *d) {
    return ((d->word << 3) & 0xFFF8) + d->vertices;
}

static u32 xem_vertex1(XemDraw *d) {
    return ((d->word >> 13) & 0xFFF8) + d->vertices;
}

static u32 xem_vertex2(XemDraw *d) {
    return (d->half << 3) + d->vertices;
}

static u32 xem_vertex3(XemDraw *d) {
    return ((u32)XEM_U16(d->record - 2) << 3) + d->vertices;
}

/* An SVECTOR into V0, V1 or V2 (two lwc2). */
static void xem_load_vector(int v, u32 address) {
    xem_lwc2(v * 2, address);
    xem_lwc2(v * 2 + 1, address + 4);
}

static u32 xem_mfc2(int reg) {
    return xem_gte_read_data(reg);
}

static void xem_cop2(u32 command) {
    xem_gte_execute(command);
}

/* The error test: LZCR, not FLAG (see above). */
static int xem_draw_error(void) {
    return (s32)xem_mfc2(XEM_GTE_LZCR) < 0;
}

/* Screen bounds (model_y_test, model_x_test): kept when some vertex has
 * 0 <= y < limit (the packed word compared unsigned with limit << 16) and
 * some vertex, not necessarily the same, 0 <= x < limit. */
static int xem_y_in(XemDraw *d, u32 sxy) {
    return sxy < d->y_limit;
}

static int xem_x_in(XemDraw *d, u32 sxy) {
    return (sxy & 0xFFFF) < d->x_limit;
}

static int xem_y_test3(XemDraw *d, u32 a, u32 b, u32 c) {
    return xem_y_in(d, a) || xem_y_in(d, b) || xem_y_in(d, c);
}

static int xem_x_test3(XemDraw *d, u32 a, u32 b, u32 c) {
    return xem_x_in(d, a) || xem_x_in(d, b) || xem_x_in(d, c);
}

static int xem_y_test4(XemDraw *d, u32 a, u32 b, u32 c, u32 e) {
    return xem_y_test3(d, a, b, c) || xem_y_in(d, e);
}

static int xem_x_test4(XemDraw *d, u32 a, u32 b, u32 c, u32 e) {
    return xem_x_test3(d, a, b, c) || xem_x_in(d, e);
}

/* The OT entry of a depth: srav by the shift, sll 2, addu the table. */
static u32 xem_ot_entry(XemDraw *d, u32 depth, s32 shift) {
    return d->ot + ((u32)((s32)depth >> (shift & 31)) << 2);
}

/* Link the slot at an OT entry whose old word was read as `old`. */
static void xem_link(XemDraw *d, u32 entry, u32 old, u32 tag) {
    XEM_U32(entry) = d->slot;
    XEM_U32(d->slot) = old | tag;
}

/* The three (or four) SXY words, `step` bytes apart from slot + 8. */
static void xem_store_xy3(XemDraw *d, u32 step, u32 a, u32 b, u32 c) {
    XEM_U32(d->slot + 8) = a;
    XEM_U32(d->slot + step + 8) = b;
    XEM_U32(d->slot + step * 2 + 8) = c;
}

/* A depth step of the far and near sorts: the branch after slt of
 * candidate < current keeps the current depth (bnez: the largest, beqz:
 * the smallest); the comparison is signed. */
static u32 xem_depth_keep(u32 current, u32 candidate, int far) {
    int less = (s32)candidate < (s32)current;

    if (far ? less : !less) {
        return current;
    }
    return candidate;
}

/* A colour word: the 24-bit RGB with a code byte. */
static u32 xem_colour(u32 code, u32 rgb) {
    return code | (rgb & 0x00FFFFFF);
}

/* model_draw_ft3_envmap's patch base (800308D0): the code of its texture
 * coordinate mapping, which model_set_envmap_mapping rewrites. */
extern u8 model_envmap_patch_base[];

/* Offsets from model_envmap_patch_base of model_draw_ft3_envmap's six srl
 * instructions (u0, u1, u2, then v0, v1, v2) and of the addiu after each. */
static const u8 xem_envmap_u_shifts[3] = { 0x30, 0x5C, 0x7C };
static const u8 xem_envmap_v_shifts[3] = { 0x3C, 0x68, 0x88 };
#define XEM_ENVMAP_OFFSET(shift) ((shift) + 4)

/* Replace the sa field (bits 6-10) of the srl at `address` with `count`, as
 * the original's halfword read, mask, or and store do. */
static void xem_envmap_patch_shift(u32 address, u32 count) {
    XEM_U16(address) = (u16)((XEM_U16(address) & 0xF83F) | count);
}

/* Set the environment-map mapping by rewriting model_draw_ft3_envmap's code
 * in game memory: the shift counts u_shift and v_shift (0..31) into its six
 * srl instructions, the offsets into the immediates of its six addiu. The
 * image holds (6, 6, 0x40, 0x40). */
void model_set_envmap_mapping(s32 u_shift, s32 v_shift, s32 u_offset, s32 v_offset) {
    u32 base = XEM_ADDRESS_OF(model_envmap_patch_base[0]);
    u32 u = (u32)u_shift << 6;
    u32 v = (u32)v_shift << 6;
    s32 i;

    for (i = 0; i < 3; i++) {
        xem_envmap_patch_shift(base + xem_envmap_u_shifts[i], u);
    }
    for (i = 0; i < 3; i++) {
        xem_envmap_patch_shift(base + xem_envmap_v_shifts[i], v);
    }
    for (i = 0; i < 3; i++) {
        XEM_U16(base + XEM_ENVMAP_OFFSET(xem_envmap_u_shifts[i])) = (u16)u_offset;
    }
    for (i = 0; i < 3; i++) {
        XEM_U16(base + XEM_ENVMAP_OFFSET(xem_envmap_v_shifts[i])) = (u16)v_offset;
    }
}

/* ---------------------------------------------------------------- AVSZ sorts */

/* model_draw_gt3_avg.s, triangles: kept when on screen and NCLIP > 0; the
 * three SXY words are written and the face counted, then linked at
 * OT[AVSZ3 >> shift] unless the OTZ is 0. */
static void xem_draw_avg_triangles(u8 *records, s32 count, u32 step, u32 tag, u32 size) {
    XemDraw d;
    u32 sxy0, sxy1, sxy2, otz, entry, old;
    s32 nclip;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    xem_load_vector(0, xem_vertex0(&d));
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= size;
    for (;;) {
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += size;
        xem_load_vector(0, xem_vertex0(&d));
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        if (!xem_y_test3(&d, sxy0, sxy1, sxy2) || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        xem_cop2(XEM_GTE_AVSZ3);
        d.slot &= XEM_DMA_MASK;
        if (nclip <= 0) {
            continue;
        }
        xem_store_xy3(&d, step, sxy0, sxy1, sxy2);
        otz = xem_mfc2(XEM_GTE_OTZ);
        d.drawn++;
        if (otz == 0) {
            continue;
        }
        entry = xem_ot_entry(&d, otz, d.shift);
        old = XEM_U32(entry);
        xem_link(&d, entry, old, tag);
    }
    xem_draw_end(&d, size);
}

/* model_draw_gt3_avg.s, quads: RTPT projects the first three points and
 * NCLIP tests their winding only; RTPS then projects the fourth. A quad on
 * screen is counted, and only then skipped when its AVSZ4 OTZ is 0;
 * otherwise it is linked and its four SXY words written. */
static void xem_draw_avg_quads(u8 *records, s32 count, u32 step, u32 tag, u32 size) {
    XemDraw d;
    u32 first, fourth, sxy0, sxy1, sxy2, sxy3, otz, entry, old;
    s32 nclip;
    int error;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    first = xem_vertex0(&d);
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= size;
    for (;;) {
        xem_load_vector(0, first);
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += size;
        first = xem_vertex0(&d);
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        error = xem_draw_error();
        xem_cop2(XEM_GTE_NCLIP);
        d.slot &= XEM_DMA_MASK;
        if (error) {
            continue;
        }
        fourth = xem_vertex3(&d);
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        if (nclip <= 0) {
            continue;
        }
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        xem_load_vector(0, fourth);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_RTPS);
        error = xem_draw_error();
        sxy3 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_AVSZ4);
        if (error) {
            continue;
        }
        if (!xem_y_test4(&d, sxy0, sxy1, sxy2, sxy3) || !xem_x_test4(&d, sxy0, sxy1, sxy2, sxy3)) {
            continue;
        }
        otz = xem_mfc2(XEM_GTE_OTZ);
        d.drawn++;
        if (otz == 0) {
            continue;
        }
        entry = xem_ot_entry(&d, otz, d.shift);
        old = XEM_U32(entry);
        xem_link(&d, entry, old, tag);
        XEM_U32(d.slot + 8) = sxy0;
        XEM_U32(d.slot + step + 8) = sxy1;
        XEM_U32(d.slot + step * 2 + 8) = sxy2;
        XEM_U32(d.slot + step * 3 + 8) = sxy3;
    }
    xem_draw_end(&d, size);
}

/* The entry points select the packet format: XY word step, tag length and
 * packet size (model_packet). */
void model_draw_gt3_avg(u8 *records, s32 count) { xem_draw_avg_triangles(records, count, 12, 0x09000000, 40); }
void model_draw_g3_avg(u8 *records, s32 count) { xem_draw_avg_triangles(records, count, 8, 0x06000000, 28); }
void model_draw_f3_avg(u8 *records, s32 count) { xem_draw_avg_triangles(records, count, 4, 0x04000000, 20); }
void model_draw_ft3_avg(u8 *records, s32 count) { xem_draw_avg_triangles(records, count, 8, 0x07000000, 32); }
void model_draw_gt4_avg(u8 *records, s32 count) { xem_draw_avg_quads(records, count, 12, 0x0C000000, 52); }
void model_draw_g4_avg(u8 *records, s32 count) { xem_draw_avg_quads(records, count, 8, 0x08000000, 36); }
void model_draw_f4_avg(u8 *records, s32 count) { xem_draw_avg_quads(records, count, 4, 0x05000000, 24); }
void model_draw_ft4_avg(u8 *records, s32 count) { xem_draw_avg_quads(records, count, 8, 0x09000000, 40); }

/* ---------------------------------------------------------------- far and near sorts */

/* model_depth.s, triangles: sorted by the largest (far) or smallest (near)
 * of SZ1..SZ3 shifted by model_ot_depth_shift + 2. The first SXY word is
 * written also for a back face; a front-facing face gets all three, is
 * counted and, unless its depth is 0, linked. */
static void xem_draw_depth_triangles(u8 *records, s32 count, u32 step, u32 tag, u32 size, int far) {
    XemDraw d;
    u32 sxy0, sxy1, sxy2, sz2, sz3, depth, entry, old;
    s32 nclip;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    d.shift += 2;
    xem_load_vector(0, xem_vertex0(&d));
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= size;
    for (;;) {
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += size;
        xem_load_vector(0, xem_vertex0(&d));
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        if (!xem_y_test3(&d, sxy0, sxy1, sxy2) || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        sz2 = xem_mfc2(XEM_GTE_SZ2);
        XEM_U32(d.slot + 8) = sxy0;
        if (nclip <= 0) {
            continue;
        }
        XEM_U32(d.slot + step + 8) = sxy1;
        XEM_U32(d.slot + step * 2 + 8) = sxy2;
        depth = xem_mfc2(XEM_GTE_SZ1);
        sz3 = xem_mfc2(XEM_GTE_SZ3);
        depth = xem_depth_keep(depth, sz2, far);
        d.slot &= XEM_DMA_MASK;
        depth = xem_depth_keep(depth, sz3, far);
        d.drawn++;
        if (depth == 0) {
            continue;
        }
        entry = xem_ot_entry(&d, depth, d.shift);
        old = XEM_U32(entry);
        xem_link(&d, entry, old, tag);
    }
    xem_draw_end(&d, size);
}

/* model_depth.s, quads: once on screen the SXY words are written (the
 * first as soon as the y test passes, whatever the x test says: the x
 * test's accept instruction is its reject branch's delay slot), but a zero
 * SZ at any of the four points rejects the quad before it is counted or
 * linked (the fourth SXY is written only once SZ0 is nonzero). */
static void xem_draw_depth_quads(u8 *records, s32 count, u32 step, u32 tag, u32 size, int far) {
    XemDraw d;
    u32 first, fourth, sxy0, sxy1, sxy2, sxy3, sz0, sz1, sz2, sz3, depth, entry, old;
    s32 nclip;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    d.shift += 2;
    first = xem_vertex0(&d);
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= size;
    for (;;) {
        xem_load_vector(0, first);
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += size;
        first = xem_vertex0(&d);
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        fourth = xem_vertex3(&d);
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        if (nclip <= 0) {
            continue;
        }
        xem_load_vector(0, fourth);
        xem_cop2(XEM_GTE_RTPS);
        if (xem_draw_error()) {
            continue;
        }
        sxy3 = xem_mfc2(XEM_GTE_SXY2);
        if (!xem_y_test4(&d, sxy0, sxy1, sxy2, sxy3)) {
            continue;
        }
        XEM_U32(d.slot + 8) = sxy0;
        if (!xem_x_test4(&d, sxy0, sxy1, sxy2, sxy3)) {
            continue;
        }
        XEM_U32(d.slot + step + 8) = sxy1;
        sz0 = xem_mfc2(XEM_GTE_SZ0);
        XEM_U32(d.slot + step * 2 + 8) = sxy2;
        if (sz0 == 0) {
            continue;
        }
        sz1 = xem_mfc2(XEM_GTE_SZ1);
        XEM_U32(d.slot + step * 3 + 8) = sxy3;
        if (sz1 == 0) {
            continue;
        }
        depth = xem_depth_keep(sz0, sz1, far);
        sz2 = xem_mfc2(XEM_GTE_SZ2);
        if (sz2 == 0) {
            continue;
        }
        sz3 = xem_mfc2(XEM_GTE_SZ3);
        depth = xem_depth_keep(depth, sz2, far);
        d.slot &= XEM_DMA_MASK;
        if (sz3 == 0) {
            continue;
        }
        depth = xem_depth_keep(depth, sz3, far);
        d.drawn++;
        if (depth == 0) {
            continue;
        }
        entry = xem_ot_entry(&d, depth, d.shift);
        old = XEM_U32(entry);
        xem_link(&d, entry, old, tag);
    }
    xem_draw_end(&d, size);
}

void model_draw_gt3_far(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 12, 0x09000000, 40, 1); }
void model_draw_g3_far(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 8, 0x06000000, 28, 1); }
void model_draw_f3_far(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 4, 0x04000000, 20, 1); }
void model_draw_ft3_far(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 8, 0x07000000, 32, 1); }
void model_draw_gt3_near(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 12, 0x09000000, 40, 0); }
void model_draw_g3_near(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 8, 0x06000000, 28, 0); }
void model_draw_f3_near(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 4, 0x04000000, 20, 0); }
void model_draw_ft3_near(u8 *records, s32 count) { xem_draw_depth_triangles(records, count, 8, 0x07000000, 32, 0); }
void model_draw_gt4_far(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 12, 0x0C000000, 52, 1); }
void model_draw_g4_far(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 8, 0x08000000, 36, 1); }
void model_draw_f4_far(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 4, 0x05000000, 24, 1); }
void model_draw_ft4_far(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 8, 0x09000000, 40, 1); }
void model_draw_gt4_near(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 12, 0x0C000000, 52, 0); }
void model_draw_g4_near(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 8, 0x08000000, 36, 0); }
void model_draw_f4_near(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 4, 0x05000000, 24, 0); }
void model_draw_ft4_near(u8 *records, s32 count) { xem_draw_depth_quads(records, count, 8, 0x09000000, 40, 0); }

/* ---------------------------------------------------------------- lit and depth-cued */

/* model_draw_f3_lit.s: flat triangles (POLY_F3) lit from the lit-colour
 * cache, twelve bytes per face (colour word, face normal), consumed per
 * face culled or not. The SXY words are written before the NCLIP test; a
 * front-facing face is counted, lit with NCCS and always linked (no zero
 * test); its colour word takes the lit RGB and the cached code byte. */
void model_draw_f3_lit(u8 *records, s32 count) {
    XemDraw d;
    u32 first, cache, sxy0, sxy1, sxy2, otz, colour, entry, old;
    s32 nclip;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    first = xem_vertex0(&d);
    cache = model_lit_color_cache;
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= 20;
    for (;;) {
        xem_load_vector(0, first);
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += 20;
        first = xem_vertex0(&d);
        cache += 12;
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        if (!xem_y_test3(&d, sxy0, sxy1, sxy2) || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        XEM_U32(d.slot + 8) = sxy0;
        xem_cop2(XEM_GTE_AVSZ3);
        XEM_U32(d.slot + 12) = sxy1;
        XEM_U32(d.slot + 16) = sxy2;
        if (nclip <= 0) {
            continue;
        }
        d.slot &= XEM_DMA_MASK;
        otz = xem_mfc2(XEM_GTE_OTZ);
        d.drawn++;
        colour = XEM_U32(cache - 12);
        xem_load_vector(0, cache - 8);
        xem_gte_write_data(XEM_GTE_RGBC, colour);
        xem_cop2(XEM_GTE_NCCS);
        entry = xem_ot_entry(&d, otz, d.shift);
        XEM_U32(d.slot + 4) = xem_colour(colour & 0xFF000000, xem_mfc2(XEM_GTE_RGB2));
        old = XEM_U32(entry);
        xem_link(&d, entry, old, 0x04000000);
    }
    model_lit_color_cache = cache;
    xem_draw_end(&d, 20);
}

/* The colour word of a depth-cued face: the DPCS result with the packet's
 * own code less bit 0, so that a texture is modulated. */
static u32 xem_cued_colour(XemDraw *d) {
    u32 code = (u32)XEM_U8(d->slot + 7) << 24;

    return xem_colour(code & 0xFE000000, xem_mfc2(XEM_GTE_RGB2));
}

/* model_draw_f3_cued.s: depth-cued triangles sorted by AVSZ3. RGBC holds
 * the model colour; faces are culled, written, counted and linked as
 * model_draw_gt3_avg's triangles, and DPCS (by the IR0 of the RTPT's last
 * vertex) is issued for every counted face, linked or not. */
static void xem_draw_cued_triangles(u8 *records, s32 count, u32 step, u32 tag, u32 size) {
    XemDraw d;
    u32 sxy0, sxy1, sxy2, otz, entry, old;
    s32 nclip;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    xem_load_vector(0, xem_vertex0(&d));
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= size;
    xem_gte_write_data(XEM_GTE_RGBC, model_color);
    for (;;) {
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += size;
        xem_load_vector(0, xem_vertex0(&d));
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        if (!xem_y_test3(&d, sxy0, sxy1, sxy2) || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        xem_cop2(XEM_GTE_AVSZ3);
        d.slot &= XEM_DMA_MASK;
        if (nclip <= 0) {
            continue;
        }
        xem_store_xy3(&d, step, sxy0, sxy1, sxy2);
        otz = xem_mfc2(XEM_GTE_OTZ);
        d.drawn++;
        xem_cop2(XEM_GTE_DPCS);
        if (otz == 0) {
            continue;
        }
        entry = xem_ot_entry(&d, otz, d.shift);
        XEM_U32(d.slot + 4) = xem_cued_colour(&d);
        old = XEM_U32(entry);
        xem_link(&d, entry, old, tag);
    }
    xem_draw_end(&d, size);
}

void model_draw_f3_cued(u8 *records, s32 count) { xem_draw_cued_triangles(records, count, 4, 0x04000000, 20); }
void model_draw_ft3_cued(u8 *records, s32 count) { xem_draw_cued_triangles(records, count, 8, 0x07000000, 32); }

/* model_draw_ft3_cued_far.s: depth-cued POLY_FT3 sorted by the largest of
 * SZ1..SZ3 (model_ot_depth_shift + 2). DPCS is issued once the y test is
 * reached (it is the test's accept instruction and its reject branch's
 * delay slot); a front-facing face on screen is written and counted, and
 * left unlinked at depth 0. */
void model_draw_ft3_cued_far(u8 *records, s32 count) {
    XemDraw d;
    u32 sxy0, sxy1, sxy2, sz2, sz3, depth, code, rgb, entry, old;
    s32 nclip;
    int y;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    d.shift += 2;
    xem_load_vector(0, xem_vertex0(&d));
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= 32;
    xem_gte_write_data(XEM_GTE_RGBC, model_color);
    for (;;) {
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += 32;
        xem_load_vector(0, xem_vertex0(&d));
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        y = xem_y_test3(&d, sxy0, sxy1, sxy2);
        xem_cop2(XEM_GTE_DPCS);
        if (!y || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        sz2 = xem_mfc2(XEM_GTE_SZ2);
        d.slot &= XEM_DMA_MASK;
        if (nclip <= 0) {
            continue;
        }
        xem_store_xy3(&d, 8, sxy0, sxy1, sxy2);
        sz3 = xem_mfc2(XEM_GTE_SZ3);
        depth = xem_depth_keep(xem_mfc2(XEM_GTE_SZ1), sz2, 1);
        code = XEM_U8(d.slot + 7);
        depth = xem_depth_keep(depth, sz3, 1);
        d.drawn++;
        rgb = xem_mfc2(XEM_GTE_RGB2);
        if (depth == 0) {
            continue;
        }
        entry = xem_ot_entry(&d, depth, d.shift);
        XEM_U32(d.slot + 4) = xem_colour((code << 24) & 0xFE000000, rgb);
        old = XEM_U32(entry);
        xem_link(&d, entry, old, 0x07000000);
    }
    xem_draw_end(&d, 32);
}

/* model_draw_ft3_lit.s: POLY_FT3 lit by cached face normals (eight bytes
 * per face, consumed culled or not). The SXY words are stored around the
 * NCLIP test, the first two also for a back face. A front-facing face is
 * counted and, unless its AVSZ3 OTZ is 0, lit by NCS and linked; its colour
 * word keeps the packet's code byte. The cached normal's first word is
 * loaded into VXY0 also for a zero OTZ (a delay slot). */
void model_draw_ft3_lit(u8 *records, s32 count) {
    XemDraw d;
    u32 first, cache, sxy0, sxy1, sxy2, otz, entry, old;
    s32 nclip;

    xem_draw_begin(&d, records, count);
    cache = model_lit_color_cache;
    xem_draw_read(&d);
    first = xem_vertex0(&d);
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= 32;
    for (;;) {
        xem_load_vector(0, first);
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += 32;
        first = xem_vertex0(&d);
        cache += 8;
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        if (!xem_y_test3(&d, sxy0, sxy1, sxy2) || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        XEM_U32(d.slot + 8) = sxy0;
        XEM_U32(d.slot + 16) = sxy1;
        if (nclip <= 0) {
            continue;
        }
        xem_cop2(XEM_GTE_AVSZ3);
        XEM_U32(d.slot + 24) = sxy2;
        d.slot &= XEM_DMA_MASK;
        otz = xem_mfc2(XEM_GTE_OTZ);
        d.drawn++;
        xem_lwc2(XEM_GTE_VXY0, cache - 8);
        if (otz == 0) {
            continue;
        }
        xem_lwc2(XEM_GTE_VZ0, cache - 4);
        xem_cop2(XEM_GTE_NCS);
        entry = xem_ot_entry(&d, otz, d.shift);
        XEM_U32(d.slot + 4) = xem_colour((u32)XEM_U8(d.slot + 7) << 24, xem_mfc2(XEM_GTE_RGB2));
        old = XEM_U32(entry);
        xem_link(&d, entry, old, 0x07000000);
    }
    model_lit_color_cache = cache;
    xem_draw_end(&d, 32);
}

/* The vertex-normal renderers' setup: the face's three vertex addresses
 * (t6..t8) and normals - vertices (a3; the original's trapping sub). */
typedef struct {
    u32 vertex[3];
    u32 delta;
} XemNormals;

static void xem_normals_next(XemDraw *d, XemNormals *n) {
    n->vertex[0] = xem_vertex0(d);
    n->vertex[1] = xem_vertex1(d);
    n->vertex[2] = xem_vertex2(d);
}

/* Load the face's vertices, and after the count test spill its normal
 * addresses to the scratchpad words 0x1F800000..8. */
static void xem_normals_project(XemNormals *n) {
    xem_load_vector(0, n->vertex[0]);
    xem_load_vector(1, n->vertex[1]);
    xem_load_vector(2, n->vertex[2]);
    xem_cop2(XEM_GTE_RTPT);
}

static void xem_normals_spill(XemNormals *n) {
    XEM_U32(XEM_SPILL) = n->vertex[0] + n->delta;
    XEM_U32(XEM_SPILL + 4) = n->vertex[1] + n->delta;
    XEM_U32(XEM_SPILL + 8) = n->vertex[2] + n->delta;
}

/* V0..V2 = the spilled vertex normals; `colour` (when not null) is read
 * from the cache between the loads and written to RGBC, as model_draw_g3_lit
 * does. */
static void xem_normals_load(u32 first, u32 cache, u32 *colour) {
    u32 second, third;

    xem_lwc2(XEM_GTE_VXY0, first);
    second = XEM_U32(XEM_SPILL + 4);
    xem_lwc2(XEM_GTE_VZ0, first + 4);
    third = XEM_U32(XEM_SPILL + 8);
    xem_load_vector(1, second);
    if (colour != NULL) {
        *colour = XEM_U32(cache - 4);
    }
    xem_load_vector(2, third);
    if (colour != NULL) {
        xem_gte_write_data(XEM_GTE_RGBC, *colour);
    }
}

/* model_draw_gt3_lit.s and model_draw_g3_lit.s: Gouraud triangles lit by
 * their vertex normals (model_current_normals, indexed like the vertices).
 * The SXY words are stored around the NCLIP test, the first two also for a
 * back face. A front-facing face with a nonzero AVSZ3 OTZ is counted (only
 * then), lit (NCT, or NCCT of a cached colour word consumed per face) and
 * linked. RGB0 takes the packet's (GT3) or the cached colour's (G3) code
 * byte; RGB1 and RGB2 are stored whole, their high byte landing in padding. */
static void xem_draw_vertex_lit(u8 *records, s32 count, int cached) {
    XemDraw d;
    XemNormals n;
    u32 size = cached ? 28 : 40;
    u32 cache = 0, colour = 0, sxy0, sxy1, sxy2, otz, normal, code, entry, old;
    s32 nclip;

    xem_draw_begin(&d, records, count);
    n.delta = model_current_normals;
    if (cached) {
        cache = model_lit_color_cache;
    }
    xem_draw_read(&d);
    xem_normals_next(&d, &n);
    d.slot -= size;
    n.delta -= d.vertices;
    for (;;) {
        xem_normals_project(&n);
        if (d.left == 0) {
            break;
        }
        xem_normals_spill(&n);
        d.left--;
        d.record += 8;
        if (cached) {
            cache += 4;
        }
        xem_draw_read(&d);
        d.slot += size;
        xem_normals_next(&d, &n);
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        if (!xem_y_test3(&d, sxy0, sxy1, sxy2) || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        XEM_U32(d.slot + 8) = sxy0;
        XEM_U32(d.slot + (cached ? 16 : 20)) = sxy1;
        if (nclip <= 0) {
            continue;
        }
        xem_cop2(XEM_GTE_AVSZ3);
        XEM_U32(d.slot + (cached ? 24 : 32)) = sxy2;
        d.slot &= XEM_DMA_MASK;
        otz = xem_mfc2(XEM_GTE_OTZ);
        normal = XEM_U32(XEM_SPILL);
        if (otz == 0) {
            continue;
        }
        d.drawn++;
        xem_normals_load(normal, cache, cached ? &colour : NULL);
        xem_cop2(cached ? XEM_GTE_NCCT : XEM_GTE_NCT);
        entry = xem_ot_entry(&d, otz, d.shift);
        code = cached ? colour & 0xFF000000 : (u32)XEM_U8(d.slot + 7) << 24;
        XEM_U32(d.slot + 4) = xem_colour(code, xem_mfc2(XEM_GTE_RGB0));
        xem_swc2(XEM_GTE_RGB1, d.slot + (cached ? 12 : 16));
        xem_swc2(XEM_GTE_RGB2, d.slot + (cached ? 20 : 28));
        old = XEM_U32(entry);
        xem_link(&d, entry, old, cached ? 0x06000000 : 0x09000000);
    }
    if (cached) {
        model_lit_color_cache = cache;
    }
    xem_draw_end(&d, size);
}

void model_draw_gt3_lit(u8 *records, s32 count) { xem_draw_vertex_lit(records, count, 0); }
void model_draw_g3_lit(u8 *records, s32 count) { xem_draw_vertex_lit(records, count, 1); }

/* model_draw_f4_lit.s and model_draw_ft4_lit.s: flat quads lit from the
 * cache (F4: twelve bytes per face, colour word then normal, lit by NCCS
 * with the cached code byte; FT4: an eight-byte normal, lit by NCS with the
 * packet's code byte), consumed per face culled or not. Quads are culled
 * as in model_draw_gt3_avg; one on screen is counted and, unless its AVSZ4
 * OTZ is 0, lit, written (four SXY words and the colour) and linked.
 * Inside the loop index 0 is masked to thirteen bits like index 1 (the
 * first face's is not). */
static void xem_draw_quad_lit(u8 *records, s32 count, int textured) {
    XemDraw d;
    u32 size = textured ? 40 : 24;
    u32 step = textured ? 8 : 4;
    u32 record_bytes = textured ? 8 : 12;
    u32 first, fourth, cache, sxy0, sxy1, sxy2, sxy3, otz, colour = 0, code, entry, old;
    s32 nclip;
    int error, y0;

    xem_draw_begin(&d, records, count);
    cache = model_lit_color_cache;
    xem_draw_read(&d);
    first = xem_vertex0(&d);
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= size;
    for (;;) {
        xem_load_vector(0, first);
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += size;
        first = xem_vertex0_13(&d);
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        cache += record_bytes;
        error = xem_draw_error();
        xem_cop2(XEM_GTE_NCLIP);
        d.slot &= XEM_DMA_MASK;
        if (error) {
            continue;
        }
        fourth = xem_vertex3(&d);
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        if (nclip <= 0) {
            continue;
        }
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        xem_load_vector(0, fourth);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_RTPS);
        y0 = xem_y_in(&d, sxy0);
        error = xem_draw_error();
        sxy3 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_AVSZ4);
        if (error) {
            continue;
        }
        if (!(y0 || xem_y_in(&d, sxy1) || xem_y_in(&d, sxy2) || xem_y_in(&d, sxy3)) ||
            !xem_x_test4(&d, sxy0, sxy1, sxy2, sxy3)) {
            continue;
        }
        otz = xem_mfc2(XEM_GTE_OTZ);
        d.drawn++;
        if (otz == 0) {
            continue;
        }
        if (textured) {
            xem_load_vector(0, cache - 8);
            entry = xem_ot_entry(&d, otz, d.shift);
            xem_cop2(XEM_GTE_NCS);
            old = XEM_U32(entry);
            code = (u32)XEM_U8(d.slot + 7) << 24;
        } else {
            colour = XEM_U32(cache - 12);
            xem_load_vector(0, cache - 8);
            xem_gte_write_data(XEM_GTE_RGBC, colour);
            xem_cop2(XEM_GTE_NCCS);
            entry = xem_ot_entry(&d, otz, d.shift);
            old = XEM_U32(entry);
            code = colour & 0xFF000000;
        }
        XEM_U32(d.slot + 8) = sxy0;
        XEM_U32(d.slot + step + 8) = sxy1;
        XEM_U32(d.slot + step * 2 + 8) = sxy2;
        XEM_U32(d.slot + step * 3 + 8) = sxy3;
        XEM_U32(d.slot + 4) = xem_colour(code, xem_mfc2(XEM_GTE_RGB2));
        xem_link(&d, entry, old, textured ? 0x09000000 : 0x05000000);
    }
    model_lit_color_cache = cache;
    xem_draw_end(&d, size);
}

void model_draw_f4_lit(u8 *records, s32 count) { xem_draw_quad_lit(records, count, 0); }
void model_draw_ft4_lit(u8 *records, s32 count) { xem_draw_quad_lit(records, count, 1); }

/* model_draw_ft4_cued.s and model_draw_ft4_cued_far.s: depth-cued POLY_FT4
 * (RGBC = the model colour, DPCS by the IR0 of the fourth point), culled
 * as in model_draw_gt3_avg. The first SXY word is written once the y test
 * passes, whatever the x test says (the x test's accept instruction is its
 * reject branch's delay slot); a quad on screen gets the others and is
 * counted. AVSZ4 sorts: DPCS for every counted quad, linked unless the OTZ
 * is 0. Far sort: the largest of SZ0..SZ3 (no zero test per point) shifted
 * by model_ot_depth_shift + 2, and DPCS issued once the y test is reached. */
static void xem_draw_cued_quads(u8 *records, s32 count, int far) {
    XemDraw d;
    u32 first, fourth, sxy0, sxy1, sxy2, sxy3, depth, code, rgb, entry, old;
    s32 nclip;
    int error, y;

    xem_draw_begin(&d, records, count);
    xem_draw_read(&d);
    if (far) {
        d.shift += 2;
    }
    first = xem_vertex0(&d);
    xem_load_vector(1, xem_vertex1(&d));
    xem_load_vector(2, xem_vertex2(&d));
    d.slot -= 40;
    xem_gte_write_data(XEM_GTE_RGBC, model_color);
    for (;;) {
        xem_load_vector(0, first);
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += 40;
        first = xem_vertex0(&d);
        xem_load_vector(1, xem_vertex1(&d));
        xem_load_vector(2, xem_vertex2(&d));
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        fourth = xem_vertex3(&d);
        d.slot &= XEM_DMA_MASK;
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        if (nclip <= 0) {
            continue;
        }
        xem_load_vector(0, fourth);
        xem_cop2(XEM_GTE_RTPS);
        error = xem_draw_error();
        sxy3 = xem_mfc2(XEM_GTE_SXY2);
        if (!far) {
            xem_cop2(XEM_GTE_AVSZ4);
        }
        if (error) {
            continue;
        }
        y = xem_y_test4(&d, sxy0, sxy1, sxy2, sxy3);
        if (far) {
            xem_cop2(XEM_GTE_DPCS);
        }
        if (!y) {
            continue;
        }
        XEM_U32(d.slot + 8) = sxy0;
        if (!xem_x_test4(&d, sxy0, sxy1, sxy2, sxy3)) {
            continue;
        }
        XEM_U32(d.slot + 16) = sxy1;
        XEM_U32(d.slot + 24) = sxy2;
        XEM_U32(d.slot + 32) = sxy3;
        if (far) {
            depth = xem_depth_keep(xem_mfc2(XEM_GTE_SZ0), xem_mfc2(XEM_GTE_SZ1), 1);
            code = XEM_U8(d.slot + 7);
            depth = xem_depth_keep(depth, xem_mfc2(XEM_GTE_SZ2), 1);
            rgb = xem_mfc2(XEM_GTE_RGB2);
            depth = xem_depth_keep(depth, xem_mfc2(XEM_GTE_SZ3), 1);
            d.drawn++;
            if (depth == 0) {
                continue;
            }
            entry = xem_ot_entry(&d, depth, d.shift);
            XEM_U32(d.slot + 4) = xem_colour((code << 24) & 0xFE000000, rgb);
        } else {
            depth = xem_mfc2(XEM_GTE_OTZ);
            d.drawn++;
            xem_cop2(XEM_GTE_DPCS);
            if (depth == 0) {
                continue;
            }
            entry = xem_ot_entry(&d, depth, d.shift);
            XEM_U32(d.slot + 4) = xem_cued_colour(&d);
        }
        old = XEM_U32(entry);
        xem_link(&d, entry, old, 0x09000000);
    }
    xem_draw_end(&d, 40);
}

void model_draw_ft4_cued(u8 *records, s32 count) { xem_draw_cued_quads(records, count, 0); }
void model_draw_ft4_cued_far(u8 *records, s32 count) { xem_draw_cued_quads(records, count, 1); }

/* ---------------------------------------------------------------- environment map */

/* The sa field of the srl and the sign-extended immediate of the addiu at
 * a patch site: model_set_envmap_mapping rewrites these in the code bytes
 * in game memory, so the mapping persists across modes as the original's
 * self-modified code does, and the renderer reads it back on every call.
 * Only these fields are interpreted (the patcher keeps the rest of each
 * word for counts 0-31). */
typedef struct {
    u32 shift;
    u32 offset;
} XemEnvmapMap;

static XemEnvmapMap xem_envmap_site(u32 address) {
    XemEnvmapMap map;

    map.shift = (XEM_U32(address) >> 6) & 31;
    map.offset = (u32)(s32)(s16)XEM_U16(address + 4);
    return map;
}

/* Texture coordinate byte: srl, addiu, sb. */
static u8 xem_envmap_coordinate(u32 value, XemEnvmapMap map) {
    return (u8)((value >> map.shift) + map.offset);
}

/* model_draw_ft3_envmap.s: environment-mapped POLY_FT3. Faces are culled as
 * in model_draw_gt3_avg; the SXY words are stored around the NCLIP test, the
 * first two also for a back face. A front-facing face with a nonzero AVSZ3
 * OTZ is counted (only then) and linked, each vertex getting texture
 * coordinates from its normal rotated into view space (MVMVA by the
 * rotation matrix, no translation): u = ((x >> u_shift) + u_offset) & 0xFF,
 * v likewise from y, with the shifts and offsets of its patched code. */
void model_draw_ft3_envmap(u8 *records, s32 count) {
    XemDraw d;
    XemNormals n;
    XemEnvmapMap u[3], v[3];
    u32 base = XEM_ADDRESS_OF(model_envmap_patch_base[0]);
    u32 normal[3], sxy0, sxy1, sxy2, otz, entry, old, x, y;
    s32 nclip;
    s32 i;

    for (i = 0; i < 3; i++) {
        u[i] = xem_envmap_site(base + xem_envmap_u_shifts[i]);
        v[i] = xem_envmap_site(base + xem_envmap_v_shifts[i]);
    }
    xem_draw_begin(&d, records, count);
    n.delta = model_current_normals;
    xem_draw_read(&d);
    xem_normals_next(&d, &n);
    xem_load_vector(1, n.vertex[1]);
    xem_load_vector(2, n.vertex[2]);
    d.slot -= 32;
    n.delta -= d.vertices;
    for (;;) {
        xem_load_vector(0, n.vertex[0]);
        xem_cop2(XEM_GTE_RTPT);
        if (d.left == 0) {
            break;
        }
        for (i = 0; i < 3; i++) {
            normal[i] = n.vertex[i] + n.delta;
        }
        d.left--;
        xem_draw_next(&d);
        d.slot += 32;
        xem_normals_next(&d, &n);
        xem_load_vector(1, n.vertex[1]);
        xem_load_vector(2, n.vertex[2]);
        if (xem_draw_error()) {
            continue;
        }
        sxy0 = xem_mfc2(XEM_GTE_SXY0);
        sxy1 = xem_mfc2(XEM_GTE_SXY1);
        sxy2 = xem_mfc2(XEM_GTE_SXY2);
        xem_cop2(XEM_GTE_NCLIP);
        if (!xem_y_test3(&d, sxy0, sxy1, sxy2) || !xem_x_test3(&d, sxy0, sxy1, sxy2)) {
            continue;
        }
        nclip = (s32)xem_mfc2(XEM_GTE_MAC0);
        XEM_U32(d.slot + 8) = sxy0;
        XEM_U32(d.slot + 16) = sxy1;
        if (nclip <= 0) {
            continue;
        }
        xem_cop2(XEM_GTE_AVSZ3);
        XEM_U32(d.slot + 24) = sxy2;
        otz = xem_mfc2(XEM_GTE_OTZ);
        d.slot &= XEM_DMA_MASK;
        xem_lwc2(XEM_GTE_VXY0, normal[0]);
        if (otz == 0) {
            continue;
        }
        xem_lwc2(XEM_GTE_VZ0, normal[0] + 4);
        xem_cop2(XEM_GTE_MVMVA(1, 0, 0, 3, 0));
        d.drawn++;
        entry = xem_ot_entry(&d, otz, d.shift);
        for (i = 0; i < 3; i++) {
            x = xem_mfc2(XEM_GTE_MAC1);
            y = xem_mfc2(XEM_GTE_MAC2);
            if (i < 2) {
                xem_load_vector(0, normal[i + 1]);
                xem_cop2(XEM_GTE_MVMVA(1, 0, 0, 3, 0));
            }
            XEM_U8(d.slot + 12 + i * 8) = xem_envmap_coordinate(x, u[i]);
            XEM_U8(d.slot + 13 + i * 8) = xem_envmap_coordinate(y, v[i]);
        }
        old = XEM_U32(entry);
        xem_link(&d, entry, old, 0x07000000);
    }
    xem_draw_end(&d, 32);
}
