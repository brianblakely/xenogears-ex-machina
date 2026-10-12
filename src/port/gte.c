/* The software GTE (xem/gte.h), written from psx-spx's GTE chapter. Every
 * MAC1-MAC3 sum runs in a 44-bit accumulator: each addition is checked for
 * 44-bit overflow (FLAG bits 30-25) and wraps there, the sum is shifted by
 * sf * 12, the MAC register keeps its low 32 bits, and IR1-IR3, the colour
 * FIFO, SZ3, OTZ, SX2, SY2 and IR0 saturate from the shifted value. MAC0
 * results are checked against 32 bits (bits 16, 15). */
#include "xem/gte.h"
#include "xem/memory.h"

XemGte xem_gte;

#ifndef __psx__
XemProjectionRing xem_gte_ring_storage;
XemStoreRing xem_present_store_storage;
XemRevealRing xem_present_reveal_storage;
XemCaptureArea xem_present_capture_storage;
#endif

void xem_present_store(unsigned int address, unsigned int value, unsigned int source) {
    XemStoreRing *ring = xem_present_stores;
    XemSxyStore *s = &ring->entries[ring->count % XEM_PRESENT_STORES];

    /* Only RAM reaches the GPU's DMA (not the scratchpad, the port's stack
     * or its own data, whose low 24 bits would alias RAM); nothing while no
     * trace takes the records. */
    if ((address & 0x1FE00000u) != 0 || !xem_gte_ring->recording) {
        return;
    }
    s->address = address & 0x00FFFFFF;
    s->value = value;
    s->projections = xem_gte_ring->count;
    s->source = source;
    ring->count++;
}

void xem_present_store_at(const void *p, unsigned int value, unsigned int source) {
    xem_present_store((unsigned int)(XemUintptr)p, value, source);
}

unsigned int xem_present_sxy_source(int reg) {
    return reg >= 12 && reg <= 14 ? xem_gte_ring->fifo[reg - 12] : reg == 15 ? xem_gte_ring->fifo[2] : 0;
}

int xem_present_face_visible(unsigned int projections, unsigned int vertices, int width, int height) {
    XemProjectionRing *ring = xem_gte_ring;
    XemProjection *p;
    long long x[4], y[4], cross, z, sx, sy, h, back = xem_present_reveals->pull_back;
    double fx[4], fy[4];
    int i, near_x = 0, near_y = 0;
    int reach_x = (int)(xem_present_reveals->wanted & 0xFFFF), reach_y = (int)(xem_present_reveals->wanted >> 16);

    if (vertices < 3 || vertices > 4 || ring->count - (projections - vertices) > XEM_GTE_PROJECTIONS) {
        return 0;
    }
    for (i = 0; i < (int)vertices; i++) {
        p = &ring->entries[(projections - vertices + (unsigned int)i) % XEM_GTE_PROJECTIONS];
        h = p->h & 0xFFFF;
        if (back != 0) {
            /* Seen from `back` farther: in front of its near plane (half
             * the projection distance, as the GTE's divide allows) or not
             * at all; the winding from the unrounded points. */
            z = (long long)p->view[2] + back;
            if (z * 2 <= h) {
                return 0;
            }
            sx = (long long)p->view[0] * h / z + (short)(p->offset & 0xFFFF);
            sy = (long long)p->view[1] * h / z + (short)(p->offset >> 16);
            fx[i] = (double)p->view[0] * (double)h / (double)z;
            fy[i] = (double)p->view[1] * (double)h / (double)z;
        } else {
            /* The divide overflows (a point at or behind the near plane) when H >= SZ3 * 2. */
            if (h >= (p->sz & 0xFFFF) * 2) {
                return 0;
            }
            x[i] = (long long)p->x >> 8;
            y[i] = (long long)p->y >> 8;
            sx = p->x >> 16;
            sy = p->y >> 16;
        }
        if (sx >= -reach_x && sx < width + reach_x) {
            near_x = 1;
        }
        if (sy >= -reach_y && sy < height + reach_y) {
            near_y = 1;
        }
    }
    if (back != 0) {
        return fx[0] * fy[1] + fx[1] * fy[2] + fx[2] * fy[0] - fx[0] * fy[2] - fx[1] * fy[0] - fx[2] * fy[1] > 0.0 && near_x && near_y;
    }
    cross = x[0] * y[1] + x[1] * y[2] + x[2] * y[0] - x[0] * y[2] - x[1] * y[0] - x[2] * y[1];
    return cross > 0 && near_x && near_y;
}

int xem_present_square_visible(int x, int y, int z, int radius, int width, int height, int margin_x, int margin_y) {
    long long h = (long long)(xem_gte_read_control(26) & 0xFFFF), back = xem_present_reveals->pull_back;
    long long depth = (long long)z + back, ofx = (int)xem_gte_read_control(24) >> 16, ofy = (int)xem_gte_read_control(25) >> 16;
    long long reach_x = margin_x + (int)(xem_present_reveals->wanted & 0xFFFF);
    long long reach_y = margin_y + (int)(xem_present_reveals->wanted >> 16);

    if ((depth + radius) * 2 <= h) {
        return 0; /* wholly behind the near plane */
    }
    if ((depth - radius) * 2 <= h) {
        return 1; /* reaching through it: kept, its faces are judged one by one */
    }
    return ofx + ((long long)x - radius) * h / depth < width + reach_x && ofx + ((long long)x + radius) * h / depth > -reach_x &&
           ofy + ((long long)y - radius) * h / depth < height + reach_y && ofy + ((long long)y + radius) * h / depth > -reach_y;
}

void xem_present_reveal(unsigned int entry, unsigned int vertices, unsigned int projections, const unsigned int *words,
                        unsigned int count) {
    unsigned int head = XEM_U32(entry) & 0x00FFFFFF;

    /* Linked now, it would go in front of the entry's newest packet and be
     * drawn just before it (at the entry itself when that is the last). */
    xem_present_reveal_at(head == 0x00FFFFFF ? entry : head, vertices, projections, 0, words, count);
}

void xem_present_reveal_at(unsigned int node, unsigned int vertices, unsigned int projections, unsigned int order,
                           const unsigned int *words, unsigned int count) {
    XemRevealRing *ring = xem_present_reveals;
    XemReveal *r = &ring->entries[ring->count % XEM_PRESENT_REVEALS];
    unsigned int i;

    if (count > 12) {
        return;
    }
    r->entry = node & 0x00FFFFFF;
    r->shape = count | vertices << 8;
    r->projections = projections;
    r->order = order;
    for (i = 0; i < count; i++) {
        r->words[i] = words[i];
    }
    ring->count++;
}

void xem_present_capture_link(unsigned int entry, unsigned int slot, unsigned int tag) {
    XemCaptureArea *area = xem_present_capture;
    XemCapture *c;

    if (area->count >= XEM_PRESENT_CAPTURES) {
        return;
    }
    c = &area->entries[area->count++];
    c->entry = entry;
    c->slot = slot;
    c->tag = tag;
    c->projections = xem_gte_ring->count;
}

typedef long long gte_i64;

#define D xem_gte.data
#define C xem_gte.control

/* Data registers. */
#define VXY0 0
#define VZ0 1
#define RGBC 6
#define OTZ 7
#define IR0 8
#define IR1 9
#define SXY0 12
#define SXY1 13
#define SXY2 14
#define SXYP 15
#define SZ0 16
#define SZ3 19
#define RGB0 20
#define RGB1 21
#define RGB2 22
#define MAC0 24
#define MAC1 25
#define IRGB 28
#define ORGB 29
#define LZCS 30
#define LZCR 31

/* Control registers: the three matrices start at RT, LLM and LCM; each has
 * its translation-like vector of three words (TR, BK, FC). */
#define RT 0
#define TR 5
#define LLM 8
#define BK 13
#define LCM 16
#define FC 21
#define OFX 24
#define OFY 25
#define H 26
#define DQA 27
#define DQB 28
#define ZSF3 29
#define ZSF4 30
#define FLAG 31

/* FLAG bits; i is 1-3 (MAC1-MAC3, IR1-IR3, the colour FIFO's R, G, B). */
#define FLAG_MAC_POSITIVE(i) (0x80000000u >> (i))
#define FLAG_MAC_NEGATIVE(i) (0x10000000u >> (i))
#define FLAG_IR(i) (0x02000000u >> (i))
#define FLAG_COLOR(i) (0x00400000u >> (i))
#define FLAG_SZ_OTZ 0x00040000u
#define FLAG_DIVIDE 0x00020000u
#define FLAG_MAC0_POSITIVE 0x00010000u
#define FLAG_MAC0_NEGATIVE 0x00008000u
#define FLAG_SX2 0x00004000u
#define FLAG_SY2 0x00002000u
#define FLAG_IR0 0x00001000u
#define FLAG_WRITABLE 0x7FFFF000u
#define FLAG_ERROR 0x80000000u
#define FLAG_ERROR_BITS 0x7F87E000u /* 30-23 and 18-13 */

static int sign16(unsigned int value) { return (int)((value & 0xFFFF) ^ 0x8000) - 0x8000; }

static int low16(unsigned int value) { return sign16(value); }

static int high16(unsigned int value) { return sign16(value >> 16); }

/* Element (row, column) of the matrix whose first word is base: nine
 * halfwords packed two to a word, row by row. */
static int matrix_element(int base, int row, int column) {
    int index = row * 3 + column;
    unsigned int word = C[base + index / 2];

    return index & 1 ? high16(word) : low16(word);
}

static gte_i64 mac_check(int i, gte_i64 value) {
    if (value > 0x7FFFFFFFFFFLL) {
        C[FLAG] |= FLAG_MAC_POSITIVE(i);
    } else if (value < -0x80000000000LL) {
        C[FLAG] |= FLAG_MAC_NEGATIVE(i);
    }
    /* Keep 44 bits, sign extended. */
    value &= 0xFFFFFFFFFFFLL;
    return (value ^ 0x80000000000LL) - 0x80000000000LL;
}

static gte_i64 mac0_check(gte_i64 value) {
    if (value > 0x7FFFFFFFLL) {
        C[FLAG] |= FLAG_MAC0_POSITIVE;
    } else if (value < -0x80000000LL) {
        C[FLAG] |= FLAG_MAC0_NEGATIVE;
    }
    return value;
}

/* IRi from a shifted MAC value: -8000h..7FFFh, or 0..7FFFh with lm. */
static int ir_saturate(int i, gte_i64 value, int lm, int flag) {
    gte_i64 low = lm ? 0 : -0x8000;

    if (value < low) {
        C[FLAG] |= flag ? FLAG_IR(i) : 0;
        return (int)low;
    }
    if (value > 0x7FFF) {
        C[FLAG] |= flag ? FLAG_IR(i) : 0;
        return 0x7FFF;
    }
    return (int)value;
}

static void set_ir(int i, gte_i64 value, int lm) {
    D[IR0 + i] = (unsigned int)ir_saturate(i, value, lm, 1);
}

/* MACi and IRi from a 44-bit sum. */
static void set_mac_ir(int i, gte_i64 value, int shift, int lm) {
    value >>= shift;
    D[MAC0 + i] = (unsigned int)value;
    set_ir(i, value, lm);
}

static int ir(int i) { return (int)D[IR0 + i]; }

/* Component i (1-3) of vector v: V0-V2, or IR1-IR3 for v = 3. */
static int vector(int v, int i) {
    if (v == 3) {
        return ir(i);
    }
    if (i == 3) {
        return (int)D[VZ0 + v * 2];
    }
    return i == 1 ? low16(D[VXY0 + v * 2]) : high16(D[VXY0 + v * 2]);
}

/* Element (row, column), both 1-3, of matrix mx: RT, LLM, LCM, or for mx = 3
 * the hardware's garbage matrix: -R*10h, R*10h, IR0 / RT13 x3 / RT22 x3. */
static int matrix(int mx, int row, int column) {
    int red;

    if (mx < 3) {
        return matrix_element(mx * 8, row - 1, column - 1);
    }
    if (row == 1) {
        red = (int)(D[RGBC] & 0xFF) * 0x10;
        return column == 1 ? -red : column == 2 ? red : ir(0);
    }
    return row == 2 ? matrix_element(RT, 0, 2) : matrix_element(RT, 1, 1);
}

/* MAC1-MAC3 and IR1-IR3 = (T * 1000h + M * V) >> sf*12, with matrix mx,
 * vector v and translation cv (TR, BK, FC, or none for 3). With FC the
 * hardware adds wrongly: it computes FC*1000h + M11*V1, setting the MAC and
 * IR flags of that partial sum, then discards it and keeps M12*V2 + M13*V3. */
static void multiply_matrix_vector(int mx, int v, int cv, int shift, int lm) {
    static const int translations[4] = {TR, BK, FC, -1};
    int i;
    gte_i64 sum;
    /* The vector is latched before the rows: with v = 3 a row's result must
     * not feed the next row's IR operand. */
    int x = vector(v, 1);
    int y = vector(v, 2);
    int z = vector(v, 3);

    for (i = 1; i <= 3; i++) {
        sum = cv == 3 ? 0 : (gte_i64)(int)C[translations[cv] + i - 1] * 0x1000;
        sum = mac_check(i, sum);
        sum = mac_check(i, sum + (gte_i64)matrix(mx, i, 1) * x);
        if (cv == 2) {
            ir_saturate(i, sum >> shift, lm, 1);
            sum = 0;
        }
        sum = mac_check(i, sum + (gte_i64)matrix(mx, i, 2) * y);
        sum = mac_check(i, sum + (gte_i64)matrix(mx, i, 3) * z);
        set_mac_ir(i, sum, shift, lm);
    }
}

static unsigned int color_saturate(int i, int value) {
    if (value < 0) {
        C[FLAG] |= FLAG_COLOR(i);
        return 0;
    }
    if (value > 0xFF) {
        C[FLAG] |= FLAG_COLOR(i);
        return 0xFF;
    }
    return (unsigned int)value;
}

/* Push [MAC1/16, MAC2/16, MAC3/16, CODE] onto the colour FIFO. */
static void push_color(void) {
    unsigned int color = color_saturate(1, (int)D[MAC1] >> 4) |
                         color_saturate(2, (int)D[MAC1 + 1] >> 4) << 8 |
                         color_saturate(3, (int)D[MAC1 + 2] >> 4) << 16 | (D[RGBC] & 0xFF000000u);

    D[RGB0] = D[RGB1];
    D[RGB1] = D[RGB2];
    D[RGB2] = color;
}

/* Component i (1-3) of the colour in data register reg. */
static int color(int reg, int i) { return (int)(D[reg] >> (i - 1) * 8 & 0xFF); }

/* MAC = [R*IR1, G*IR2, B*IR3] << 4, from RGBC. */
static void color_times_ir(gte_i64 *mac) {
    int i;

    for (i = 1; i <= 3; i++) {
        mac[i - 1] = mac_check(i, (gte_i64)color(RGBC, i) * ir(i) * 16);
    }
}

/* MAC = MAC + (FC - MAC) * IR0: IR = ((FC << 12) - MAC) >> sf*12 saturated as
 * with lm = 0, then MAC = IR * IR0 + MAC. */
static void interpolate_far_color(gte_i64 *mac, int shift) {
    int i;
    gte_i64 difference;

    for (i = 1; i <= 3; i++) {
        difference = mac_check(i, (gte_i64)(int)C[FC + i - 1] * 0x1000);
        difference = mac_check(i, difference - mac[i - 1]);
        set_ir(i, difference >> shift, 0);
    }
    for (i = 1; i <= 3; i++) {
        mac[i - 1] = mac_check(i, (gte_i64)ir(i) * ir(0) + mac[i - 1]);
    }
}

/* MAC = MAC >> sf*12, IR = MAC, and the colour pushed. */
static void finish_color(gte_i64 *mac, int shift, int lm) {
    int i;

    for (i = 1; i <= 3; i++) {
        set_mac_ir(i, mac[i - 1], shift, lm);
    }
    push_color();
}

/* The light: MAC = IR = LLM * V >> sf*12, then BK * 1000h + LCM * IR. */
static void light(int v, int shift, int lm) {
    multiply_matrix_vector(1, v, 3, shift, lm);
    multiply_matrix_vector(2, 3, 1, shift, lm);
}

/* The UNR division of RTPS/RTPT: H / SZ3 as 1.16 fixed point, from a
 * reciprocal seed refined by two Newton-Raphson steps, or 1FFFFh with the
 * divide flag when H >= SZ3 * 2. */
static gte_i64 divide(unsigned int h, unsigned int sz3) {
    int z;
    gte_i64 n;
    gte_i64 d;
    gte_i64 u;
    int index;
    int seed;

    if (h >= sz3 * 2) {
        C[FLAG] |= FLAG_DIVIDE;
        return 0x1FFFF;
    }
    for (z = 0; z < 15 && !(sz3 << z & 0x8000); z++) {
    }
    n = (gte_i64)h << z;
    d = (gte_i64)sz3 << z;
    index = (int)((d - 0x7FC0) >> 7);
    seed = (0x40000 / (index + 0x100) + 1) / 2 - 0x101;
    u = (seed > 0 ? seed : 0) + 0x101;
    d = (0x2000080 - d * u) >> 8;
    d = (0x80 + d * u) >> 8;
    n = (n * d + 0x8000) >> 16;
    return n < 0x1FFFF ? n : 0x1FFFF;
}

static int clamp_projection(gte_i64 value) {
    return value < -0x7FFFFFFF ? -0x7FFFFFFF : value > 0x7FFFFFFF ? 0x7FFFFFFF : (int)value;
}

/* Presentation only: the vertex just projected (xem/gte.h), at the screen
 * point (x, y) in 16.16 from MAC1, MAC2 and the depth `z`. */
static void record_projection(gte_i64 x, gte_i64 y, gte_i64 z) {
    XemProjectionRing *ring = xem_gte_ring;
    XemProjection *p = &ring->entries[ring->count % XEM_GTE_PROJECTIONS];

    if (!ring->recording) {
        return;
    }
    p->sxy = D[SXY2];
    p->sz = (D[SZ3] & 0xFFFF) | (ring->class_ & 0xFF) << 16;
    p->x = clamp_projection(x);
    p->y = clamp_projection(y);
    p->offset = ((unsigned int)((int)C[OFX] >> 16) & 0xFFFF) | (unsigned int)((int)C[OFY] >> 16) << 16;
    p->h = C[H] & 0xFFFF;
    p->owner = ring->owner;
    p->view[0] = (int)D[MAC1];
    p->view[1] = (int)D[MAC1 + 1];
    p->view[2] = clamp_projection(z);
    ring->fifo[0] = ring->fifo[1];
    ring->fifo[1] = ring->fifo[2];
    ring->fifo[2] = ring->count + 1;
    ring->count++;
}

/* One vertex of RTPS/RTPT: V = RT * Vv + TR, the screen depth, the
 * perspective division, the screen point and, for the last vertex only,
 * the depth-cue factor IR0. */
static void transform_perspective(int v, int shift, int lm, int last) {
    gte_i64 sum[3];
    gte_i64 value;
    gte_i64 n;
    int i;
    int sx;
    int sy;

    for (i = 1; i <= 3; i++) {
        value = mac_check(i, (gte_i64)(int)C[TR + i - 1] * 0x1000);
        value = mac_check(i, value + (gte_i64)matrix(0, i, 1) * vector(v, 1));
        value = mac_check(i, value + (gte_i64)matrix(0, i, 2) * vector(v, 2));
        sum[i - 1] = mac_check(i, value + (gte_i64)matrix(0, i, 3) * vector(v, 3));
        D[MAC0 + i] = (unsigned int)(sum[i - 1] >> shift);
    }
    set_ir(1, sum[0] >> shift, lm);
    set_ir(2, sum[1] >> shift, lm);
    /* With sf = 0, IR3's flag tests MAC3 >> 12 against -8000h..7FFFh as if
     * lm were 0; IR3 itself is clamped from MAC3 by lm. */
    if (shift) {
        set_ir(3, sum[2] >> shift, lm);
    } else {
        D[IR0 + 3] = (unsigned int)ir_saturate(3, sum[2], lm, 0);
        ir_saturate(3, sum[2] >> 12, 0, 1);
    }
    /* SZ3 = MAC3 >> (1 - sf) * 12, which is the sum >> 12 either way. */
    value = sum[2] >> 12;
    if (value < 0) {
        C[FLAG] |= FLAG_SZ_OTZ;
        value = 0;
    } else if (value > 0xFFFF) {
        C[FLAG] |= FLAG_SZ_OTZ;
        value = 0xFFFF;
    }
    for (i = SZ0; i < SZ3; i++) {
        D[i] = D[i + 1];
    }
    D[SZ3] = (unsigned int)value;

    n = divide(C[H] & 0xFFFF, D[SZ3]);
    value = mac0_check(n * ir(1) + (int)C[OFX]);
    D[MAC0] = (unsigned int)value;
    value >>= 16;
    sx = value < -0x400 ? -0x400 : value > 0x3FF ? 0x3FF : (int)value;
    C[FLAG] |= sx != value ? FLAG_SX2 : 0;
    value = mac0_check(n * ir(2) + (int)C[OFY]);
    D[MAC0] = (unsigned int)value;
    value >>= 16;
    sy = value < -0x400 ? -0x400 : value > 0x3FF ? 0x3FF : (int)value;
    C[FLAG] |= sy != value ? FLAG_SY2 : 0;
    D[SXY0] = D[SXY1];
    D[SXY1] = D[SXY2];
    D[SXY2] = ((unsigned int)sx & 0xFFFF) | (unsigned int)sy << 16;
    record_projection(n * ir(1) + (int)C[OFX], n * ir(2) + (int)C[OFY], sum[2] >> 12);

    if (last) {
        value = mac0_check(n * sign16(C[DQA]) + (int)C[DQB]);
        D[MAC0] = (unsigned int)value;
        value >>= 12;
        if (value < 0) {
            C[FLAG] |= FLAG_IR0;
            value = 0;
        } else if (value > 0x1000) {
            C[FLAG] |= FLAG_IR0;
            value = 0x1000;
        }
        D[IR0] = (unsigned int)value;
    }
}

static void normal_clip(void) {
    gte_i64 x0 = low16(D[SXY0]), y0 = high16(D[SXY0]);
    gte_i64 x1 = low16(D[SXY1]), y1 = high16(D[SXY1]);
    gte_i64 x2 = low16(D[SXY2]), y2 = high16(D[SXY2]);

    D[MAC0] = (unsigned int)mac0_check(x0 * y1 + x1 * y2 + x2 * y0 - x0 * y2 - x1 * y0 - x2 * y1);
}

/* OTZ = ZSF * (sum of the depths) >> 12, saturated to 0..FFFFh. */
static void average_z(int scale, int first) {
    gte_i64 sum = 0;
    gte_i64 value;
    int i;

    for (i = first; i <= SZ3; i++) {
        sum += D[i];
    }
    value = mac0_check(sum * scale);
    D[MAC0] = (unsigned int)value;
    value >>= 12;
    if (value < 0) {
        C[FLAG] |= FLAG_SZ_OTZ;
        value = 0;
    } else if (value > 0xFFFF) {
        C[FLAG] |= FLAG_SZ_OTZ;
        value = 0xFFFF;
    }
    D[OTZ] = (unsigned int)value;
}

/* The derived registers: SXYP mirrors SXY2; IRGB and ORGB both read IR1-IR3
 * >> 7 saturated to 0..1Fh (no flags); FLAG bit 31 sums the error bits. */
static void refresh(void) {
    unsigned int collected = 0;
    int i;
    int value;

    D[SXYP] = D[SXY2];
    for (i = 1; i <= 3; i++) {
        value = ir(i) >> 7;
        value = value < 0 ? 0 : value > 0x1F ? 0x1F : value;
        collected |= (unsigned int)value << (i - 1) * 5;
    }
    D[IRGB] = collected;
    D[ORGB] = collected;
    C[FLAG] &= FLAG_WRITABLE;
    C[FLAG] |= C[FLAG] & FLAG_ERROR_BITS ? FLAG_ERROR : 0;
}

XemGte *xem_gte_state(void) { return &xem_gte; }

unsigned int xem_gte_state_size(void) { return sizeof(xem_gte); }

void xem_gte_reset(void) {
    int i;

    for (i = 0; i < 32; i++) {
        D[i] = 0;
        C[i] = 0;
    }
    D[LZCR] = 32;
    for (i = 0; i < 3; i++) {
        xem_gte_ring->fifo[i] = 0;
    }
}

void xem_gte_write_data(int reg, unsigned int value) {
    int count;

    switch (reg &= 31) {
    case VZ0:
    case VZ0 + 2:
    case VZ0 + 4:
    case IR0:
    case IR0 + 1:
    case IR0 + 2:
    case IR0 + 3:
        D[reg] = (unsigned int)sign16(value);
        break;
    case OTZ:
    case SZ0:
    case SZ0 + 1:
    case SZ0 + 2:
    case SZ3:
        D[reg] = value & 0xFFFF;
        break;
    case SXYP:
        D[SXY0] = D[SXY1];
        D[SXY1] = D[SXY2];
        D[SXY2] = value;
        xem_gte_ring->fifo[0] = xem_gte_ring->fifo[1];
        xem_gte_ring->fifo[1] = xem_gte_ring->fifo[2];
        xem_gte_ring->fifo[2] = 0;
        break;
    case SXY0:
    case SXY1:
    case SXY2:
        D[reg] = value;
        xem_gte_ring->fifo[reg - SXY0] = 0;
        break;
    case IRGB:
        D[IR1] = (value & 0x1F) << 7;
        D[IR1 + 1] = (value >> 5 & 0x1F) << 7;
        D[IR1 + 2] = (value >> 10 & 0x1F) << 7;
        break;
    case ORGB:
    case LZCR:
        break;
    case LZCS:
        /* LZCR counts the leading bits equal to the sign bit, 1..32. */
        D[LZCS] = value;
        if (value & 0x80000000u) {
            value = ~value;
        }
        for (count = 0; count < 32 && !(value & 0x80000000u); count++) {
            value <<= 1;
        }
        D[LZCR] = (unsigned int)count;
        break;
    default:
        D[reg] = value;
        break;
    }
    refresh();
}

unsigned int xem_gte_read_data(int reg) { return D[reg & 31]; }

void xem_gte_write_control(int reg, unsigned int value) {
    switch (reg &= 31) {
    case RT + 4:
    case LLM + 4:
    case LCM + 4:
    case H: /* unsigned, but read back sign extended */
    case DQA:
    case ZSF3:
    case ZSF4:
        C[reg] = (unsigned int)sign16(value);
        break;
    case FLAG:
        C[FLAG] = value;
        refresh();
        break;
    default:
        C[reg] = value;
        break;
    }
}

unsigned int xem_gte_read_control(int reg) { return C[reg & 31]; }

void xem_gte_execute(unsigned int command) {
    int shift = (int)(command >> 19 & 1) * 12;
    int mx = (int)(command >> 17 & 3);
    int v = (int)(command >> 15 & 3);
    int cv = (int)(command >> 13 & 3);
    int lm = (int)(command >> 10 & 1);
    gte_i64 mac[3];
    int i;
    int j;

    C[FLAG] = 0;
    switch (command & 0x3F) {
    case 0x01: /* RTPS */
        transform_perspective(0, shift, lm, 1);
        break;
    case 0x30: /* RTPT */
        for (i = 0; i < 3; i++) {
            transform_perspective(i, shift, lm, i == 2);
        }
        break;
    case 0x06: /* NCLIP */
        normal_clip();
        break;
    case 0x0C: /* OP: IR x the rotation matrix's diagonal */
        mac[0] = mac_check(1, (gte_i64)ir(3) * matrix(0, 2, 2));
        mac[0] = mac_check(1, mac[0] - (gte_i64)ir(2) * matrix(0, 3, 3));
        mac[1] = mac_check(2, (gte_i64)ir(1) * matrix(0, 3, 3));
        mac[1] = mac_check(2, mac[1] - (gte_i64)ir(3) * matrix(0, 1, 1));
        mac[2] = mac_check(3, (gte_i64)ir(2) * matrix(0, 1, 1));
        mac[2] = mac_check(3, mac[2] - (gte_i64)ir(1) * matrix(0, 2, 2));
        for (i = 1; i <= 3; i++) {
            set_mac_ir(i, mac[i - 1], shift, lm);
        }
        break;
    case 0x10: /* DPCS */
        for (i = 1; i <= 3; i++) {
            mac[i - 1] = (gte_i64)color(RGBC, i) << 16;
        }
        interpolate_far_color(mac, shift);
        finish_color(mac, shift, lm);
        break;
    case 0x2A: /* DPCT: DPCS on RGB0 three times, the FIFO advancing */
        for (j = 0; j < 3; j++) {
            for (i = 1; i <= 3; i++) {
                mac[i - 1] = (gte_i64)color(RGB0, i) << 16;
            }
            interpolate_far_color(mac, shift);
            finish_color(mac, shift, lm);
        }
        break;
    case 0x11: /* INTPL */
        for (i = 1; i <= 3; i++) {
            mac[i - 1] = (gte_i64)ir(i) * 0x1000;
        }
        interpolate_far_color(mac, shift);
        finish_color(mac, shift, lm);
        break;
    case 0x29: /* DCPL */
        color_times_ir(mac);
        interpolate_far_color(mac, shift);
        finish_color(mac, shift, lm);
        break;
    case 0x12: /* MVMVA */
        multiply_matrix_vector(mx, v, cv, shift, lm);
        break;
    case 0x1E: /* NCS */
    case 0x20: /* NCT */
        for (i = 0; i < ((command & 0x3F) == 0x20 ? 3 : 1); i++) {
            light(i, shift, lm);
            push_color();
        }
        break;
    case 0x1B: /* NCCS */
    case 0x3F: /* NCCT */
        for (i = 0; i < ((command & 0x3F) == 0x3F ? 3 : 1); i++) {
            light(i, shift, lm);
            color_times_ir(mac);
            finish_color(mac, shift, lm);
        }
        break;
    case 0x13: /* NCDS */
    case 0x16: /* NCDT */
        for (i = 0; i < ((command & 0x3F) == 0x16 ? 3 : 1); i++) {
            light(i, shift, lm);
            color_times_ir(mac);
            interpolate_far_color(mac, shift);
            finish_color(mac, shift, lm);
        }
        break;
    case 0x1C: /* CC */
        multiply_matrix_vector(2, 3, 1, shift, lm);
        color_times_ir(mac);
        finish_color(mac, shift, lm);
        break;
    case 0x14: /* CDP */
        multiply_matrix_vector(2, 3, 1, shift, lm);
        color_times_ir(mac);
        interpolate_far_color(mac, shift);
        finish_color(mac, shift, lm);
        break;
    case 0x28: /* SQR */
        for (i = 1; i <= 3; i++) {
            set_mac_ir(i, mac_check(i, (gte_i64)ir(i) * ir(i)), shift, lm);
        }
        break;
    case 0x3D: /* GPF: MAC = IR * IR0 */
        for (i = 1; i <= 3; i++) {
            mac[i - 1] = mac_check(i, (gte_i64)ir(i) * ir(0));
        }
        finish_color(mac, shift, lm);
        break;
    case 0x3E: /* GPL: MAC = (MAC << sf*12) + IR * IR0 */
        for (i = 1; i <= 3; i++) {
            mac[i - 1] = mac_check(i, (gte_i64)(int)D[MAC0 + i] * (1 << shift));
            mac[i - 1] = mac_check(i, mac[i - 1] + (gte_i64)ir(i) * ir(0));
        }
        finish_color(mac, shift, lm);
        break;
    case 0x2D: /* AVSZ3 */
        average_z(sign16(C[ZSF3]), SZ0 + 1);
        break;
    case 0x2E: /* AVSZ4 */
        average_z(sign16(C[ZSF4]), SZ0);
        break;
    default:
        /* The unassigned opcodes: psx-spx does not know their effect; the
         * game issues none. They only clear FLAG here. */
        break;
    }
    refresh();
}
