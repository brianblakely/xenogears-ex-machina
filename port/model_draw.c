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
