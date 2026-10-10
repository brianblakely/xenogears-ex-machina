/* PsyQ libgte for the port build, over the software GTE (xem/gte.h): every
 * member the resident links (psyq_libgte_to_libcard.c from InitGeom to
 * _patch_gte, and gpu_rotation_and_psyq_libraries.c's SetFogNearFar), and the
 * handwritten rotation routines that share their sine table
 * (gpu_build_rotation_matrix.s, gpu_get_sin.s, gpu_get_cos.s).
 *
 * Each function makes the original's GTE transfers and commands in the
 * original's order, so the registers it leaves (matrices, IR, MAC, FIFOs,
 * FLAG, LZCS/LZCR) are the original's: callers and the handwritten renderers
 * read them afterwards. Memory goes through the same word, halfword and byte
 * accesses at the same offsets, including the original's quirks (stores of a
 * sign-extended IR3 or MAC word over a structure's pad halfword, matrix stores
 * made in the original's order so that aliased arguments behave the same).
 * The CPU's 32-bit multiplies keep their low word, as mflo does. The tables
 * the SDK reads are the game's own data at their original addresses:
 * rcossin_tbl (the resident's C) and the SDK's square root, inverse square
 * root and arctangent tables and matrix stack (PsyQ library .data). */
#include "psyq/libgte.h"
#include "xem/gte.h"

extern short rcossin_tbl[4096][2];
extern short libgte_square_root_table[];
extern short libgte_inverse_square_root_table[];
extern short libgte_arctangent_table[];
extern int libgte_matrix_stack_depth; /* bytes, 0x20 per matrix */
extern unsigned int libgte_matrix_stack[];
/* The game's empty debug print (main.c), defined without parameters: the
 * original passes the message, which it ignores. */
void mode_empty_debug_print(void);

#define WORD(p, offset) (*(unsigned int *)((char *)(p) + (offset)))
#define HALF(p, offset) (*(unsigned short *)((char *)(p) + (offset)))
#define SHALF(p, offset) (*(short *)((char *)(p) + (offset)))
#define BYTE(p, offset) (*(unsigned char *)((char *)(p) + (offset)))

/* Command words, as the SDK's (mvmva selectors: sf, mx, v, cv, lm). */
#define RTPS 0x4A180001
#define RTPT 0x4A280030
#define NCLIP 0x4B400006
#define OP0 0x4B70000C
#define OP12 0x4B78000C
#define DPCS 0x4A780010
#define INTPL 0x4A980011
#define MVMVA_RT_V0_TR 0x4A480012  /* 1, RT, V0, TR, 0 */
#define MVMVA_RT_V0 0x4A486012     /* 1, RT, V0, none, 0 */
#define MVMVA_RT_IR_SF0 0x4A41E012 /* 0, RT, IR, none, 0 */
#define MVMVA_RT_IR 0x4A49E012     /* 1, RT, IR, none, 0 */
#define MVMVA_LLM_V0 0x4A4A6412    /* 1, LLM, V0, none, lm */
#define MVMVA_LCM_IR_BK 0x4A4DA412 /* 1, LCM, IR, BK, lm */
#define NCDS 0x4AE80413
#define CDP 0x4B280414
#define NCDT 0x4AF80416
#define NCCS 0x4B08041B
#define CC 0x4B38041C
#define NCS 0x4AC8041E
#define NCT 0x4AD80420
#define SQR0 0x4AA00428
#define SQR12 0x4AA80428
#define DCPL 0x4A680029
#define DPCT 0x4AF8002A
#define AVSZ3 0x4B58002D
#define AVSZ4 0x4B68002E
#define GPF0 0x4B90003D
#define GPF12 0x4B98003D
#define GPL0 0x4BA0003E
#define GPL12 0x4BA8003E
#define NCCT 0x4B18043F

static unsigned int mfc2(int reg) { return xem_gte_read_data(reg); }

static void mtc2(int reg, unsigned int value) { xem_gte_write_data(reg, value); }

static unsigned int cfc2(int reg) { return xem_gte_read_control(reg); }

static void ctc2(int reg, unsigned int value) { xem_gte_write_control(reg, value); }

static void cop2(unsigned int command) { xem_gte_execute(command); }

/* lwc2/swc2: a data register from or to the word at p + offset. */
static void lwc2(int reg, void *p, int offset) { mtc2(reg, WORD(p, offset)); }

static void swc2(int reg, void *p, int offset) { WORD(p, offset) = mfc2(reg); }

/* mult/multu then mflo: the product's low word. */
static int mul(int a, int b) { return (int)((unsigned int)a * (unsigned int)b); }

/* sllv, srav: shift amounts are taken modulo 32. */
static int sllv(int value, int shift) { return (int)((unsigned int)value << (shift & 31)); }

static int srav(int value, int shift) { return value >> (shift & 31); }

static int sign16(unsigned int value) { return (int)((value & 0xFFFF) ^ 0x8000) - 0x8000; }

/* div's quotient, with the R3000's results where C has none (the compiled
 * SDK code breaks on them first: zero divisors and 80000000h / -1). */
static int divide(int n, int d) {
    if (d == 0) {
        return n < 0 ? 1 : -1;
    }
    if (d == -1 && n == (int)0x80000000) {
        return n;
    }
    return n / d;
}

/* A table halfword at a byte offset that may lie outside the table, as the
 * original's unchecked lh. */
static int table_half(short *table, int index) { return SHALF(table, index * 2); }

/* An angle's rcossin_tbl pair, masked to 12 bits; a negative angle takes the
 * pair of its magnitude (callers negate the sine). */
static unsigned int sin_cos_pair(int angle) {
    unsigned int index = (angle < 0 ? 0u - (unsigned int)angle : (unsigned int)angle) & 0xFFF;

    return WORD(rcossin_tbl, index * 4);
}

void _patch_gte(void);

/* The GTE's start-up values: ZSF3 = 155h, ZSF4 = 100h, H = 1000, DQA =
 * -1062h, DQB = 1400000h, offset 0. The original also enables COP2 in the
 * status register and saves its return address in libgte_init_geom_return
 * around the _patch_gte call; neither has a port counterpart. */
void InitGeom(void) {
    _patch_gte();
    ctc2(29, 0x155);
    ctc2(30, 0x100);
    ctc2(26, 0x3E8);
    ctc2(27, (unsigned int)-0x1062);
    ctc2(28, 0x1400000);
    ctc2(24, 0);
    ctc2(25, 0);
}

/* The square root of a (1.0 = 1), from the leading-zero count and the
 * table of 1.0 to 4.0 in 64 steps. */
long SquareRoot0(long a) {
    int zeros;
    int even;
    int shift;
    int mantissa;

    mtc2(30, (unsigned int)a);
    zeros = (int)mfc2(31);
    if (zeros == 32) {
        return 0;
    }
    even = zeros & ~1;
    shift = (31 - even) >> 1;
    mantissa = even - 24 >= 0 ? sllv((int)a, even - 24) : srav((int)a, 24 - even);
    return (
        long)((unsigned int)sllv(table_half(libgte_square_root_table, mantissa - 0x40), shift) >>
              12);
}

/* 1/sqrt(a) as a table mantissa and a shift: 1 with *mantissa and *shift
 * set, or -1 when a has no leading zero count (a is 0 or -1). */
long libgte_inverse_square_root(long a, long *mantissa, long *shift) {
    int zeros;
    int even;
    int value;

    mtc2(30, (unsigned int)a);
    zeros = (int)mfc2(31);
    if (zeros == 32 || zeros == 0) {
        return -1;
    }
    even = zeros & ~1;
    value = even - 24 >= 0 ? sllv((int)a, even - 24) : srav((int)a, 24 - even);
    *shift = (31 - even) >> 1;
    *mantissa = table_half(libgte_inverse_square_root_table, value - 0x40);
    return 1;
}

/* The original's register-argument helper (libgte_normalize_vector_in_
 * registers): the vector, truncated to 16-bit components by the GTE, scaled
 * to length 1.0 (4096) through SQR, the inverse square root table and GPF.
 * Returns the squared length, which VectorNormal* return. */
static int normalize_vector(int *v) {
    int length;
    int zeros;
    int shift;
    int mantissa;

    mtc2(9, (unsigned int)v[0]);
    mtc2(10, (unsigned int)v[1]);
    mtc2(11, (unsigned int)v[2]);
    cop2(SQR0);
    /* The original sums with trapping adds: a squared length past 31 bits
     * raises an overflow exception there, and wraps here. */
    length = (int)(mfc2(25) + mfc2(26) + mfc2(27));
    mtc2(30, (unsigned int)length);
    zeros = (int)mfc2(31) & ~1;
    shift = (31 - zeros) >> 1;
    mantissa = zeros - 24 >= 0 ? sllv(length, zeros - 24) : srav(length, 24 - zeros);
    mtc2(8, (unsigned int)table_half(libgte_inverse_square_root_table, mantissa - 0x40));
    mtc2(9, (unsigned int)v[0]);
    mtc2(10, (unsigned int)v[1]);
    mtc2(11, (unsigned int)v[2]);
    cop2(GPF0);
    v[0] = srav((int)mfc2(25), shift);
    v[1] = srav((int)mfc2(26), shift);
    v[2] = srav((int)mfc2(27), shift);
    return length;
}

long VectorNormalS(VECTOR *v0, SVECTOR *v1) {
    int v[3];
    int length;

    v[0] = (int)WORD(v0, 0);
    v[1] = (int)WORD(v0, 4);
    v[2] = (int)WORD(v0, 8);
    length = normalize_vector(v);
    HALF(v1, 0) = (unsigned short)v[0];
    HALF(v1, 2) = (unsigned short)v[1];
    HALF(v1, 4) = (unsigned short)v[2];
    return length;
}

long VectorNormal(VECTOR *v0, VECTOR *v1) {
    int v[3];
    int length;

    v[0] = (int)WORD(v0, 0);
    v[1] = (int)WORD(v0, 4);
    v[2] = (int)WORD(v0, 8);
    length = normalize_vector(v);
    WORD(v1, 0) = (unsigned int)v[0];
    WORD(v1, 4) = (unsigned int)v[1];
    WORD(v1, 8) = (unsigned int)v[2];
    return length;
}

/* Returns the squared length in the original's v0, unused by its void
 * prototype. */
void VectorNormalSS(SVECTOR *v0, SVECTOR *v1) {
    int v[3];

    v[0] = SHALF(v0, 0);
    v[1] = SHALF(v0, 2);
    v[2] = SHALF(v0, 4);
    normalize_vector(v);
    HALF(v1, 0) = (unsigned short)v[0];
    HALF(v1, 2) = (unsigned short)v[1];
    HALF(v1, 4) = (unsigned short)v[2];
}

/* With a and b m's first two rows and c = OP(a, b), out's rows are
 * OP(b, c), b and c, each normalized (OP with sf = 1, its IR saturated). The
 * rotation diagonal is restored; b waits in V0/V1's registers while the first
 * row is normalized, as the original keeps it there. */
void libgte_orthonormalize_matrix(MATRIX *m, MATRIX *out) {
    int a0 = SHALF(m, 0), a1 = SHALF(m, 2), a2 = SHALF(m, 4);
    int b0 = SHALF(m, 6), b1 = SHALF(m, 8), b2 = SHALF(m, 10);
    unsigned int d0 = cfc2(0), d1 = cfc2(2), d2 = cfc2(4);
    int c[3];
    int v[3];

    ctc2(0, (unsigned int)a0);
    ctc2(2, (unsigned int)a1);
    ctc2(4, (unsigned int)a2);
    mtc2(11, (unsigned int)b2);
    mtc2(9, (unsigned int)b0);
    mtc2(10, (unsigned int)b1);
    cop2(OP12);
    c[0] = (int)mfc2(25);
    c[1] = (int)mfc2(26);
    c[2] = (int)mfc2(27);
    ctc2(0, (unsigned int)b0);
    ctc2(2, (unsigned int)b1);
    ctc2(4, (unsigned int)b2);
    cop2(OP12);
    mtc2(0, (unsigned int)b0);
    mtc2(1, (unsigned int)b1);
    mtc2(2, (unsigned int)b2);
    v[0] = (int)mfc2(25);
    v[1] = (int)mfc2(26);
    v[2] = (int)mfc2(27);
    ctc2(0, d0);
    ctc2(2, d1);
    ctc2(4, d2);
    normalize_vector(v);
    HALF(out, 0) = (unsigned short)v[0];
    HALF(out, 2) = (unsigned short)v[1];
    HALF(out, 4) = (unsigned short)v[2];
    v[0] = (int)mfc2(0);
    v[1] = (int)mfc2(1);
    v[2] = (int)mfc2(2);
    normalize_vector(v);
    HALF(out, 6) = (unsigned short)v[0];
    HALF(out, 8) = (unsigned short)v[1];
    HALF(out, 10) = (unsigned short)v[2];
    normalize_vector(c);
    HALF(out, 12) = (unsigned short)c[0];
    HALF(out, 14) = (unsigned short)c[1];
    HALF(out, 16) = (unsigned short)c[2];
}

/* The weighted sums v2 = (v0 * p0 + v1 * p1) >> 12 or >> 0 (GPF then GPL),
 * of long vectors (LoadAverage12, LoadAverage0), of short vectors
 * (LoadAverageShort12, LoadAverageShort0), and of two or three bytes
 * (LoadAverageByte, LoadAverageCol: MAC >> 12 whatever the shift). Each
 * returns what the original leaves in v0: LZCR, read where FLAG was meant. */
static long weighted_sum_vector(VECTOR *v0, VECTOR *v1, long p0, long p1, VECTOR *v2,
                                unsigned int gpf, unsigned int gpl) {
    unsigned int result;

    mtc2(8, (unsigned int)p0);
    lwc2(9, v0, 0);
    lwc2(10, v0, 4);
    lwc2(11, v0, 8);
    cop2(gpf);
    result = mfc2(31);
    mtc2(8, (unsigned int)p1);
    lwc2(9, v1, 0);
    lwc2(10, v1, 4);
    lwc2(11, v1, 8);
    cop2(gpl);
    swc2(9, v2, 0);
    swc2(10, v2, 4);
    swc2(11, v2, 8);
    return (long)result;
}

long libgte_weighted_sum_vector12(VECTOR *v0, VECTOR *v1, long p0, long p1, VECTOR *v2) {
    return weighted_sum_vector(v0, v1, p0, p1, v2, GPF12, GPL12);
}

long libgte_weighted_sum_vector0(VECTOR *v0, VECTOR *v1, long p0, long p1, VECTOR *v2) {
    return weighted_sum_vector(v0, v1, p0, p1, v2, GPF0, GPL0);
}

/* A short vector's x (zero extended), y (its word >> 16) and z into IR. */
static void load_svector_ir(SVECTOR *v) {
    unsigned int xy = WORD(v, 0);
    unsigned int z = WORD(v, 4) & 0xFFFF;

    mtc2(9, xy & 0xFFFF);
    mtc2(10, (unsigned int)((int)xy >> 16));
    mtc2(11, z);
}

static long weighted_sum_svector(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2,
                                 unsigned int gpf, unsigned int gpl) {
    unsigned int result;
    unsigned int xy;

    mtc2(8, (unsigned int)p0);
    load_svector_ir(v0);
    cop2(gpf);
    result = mfc2(31);
    mtc2(8, (unsigned int)p1);
    load_svector_ir(v1);
    cop2(gpl);
    xy = (mfc2(9) & 0xFFFF) | mfc2(10) << 16;
    WORD(v2, 0) = xy;
    WORD(v2, 4) = mfc2(11);
    return (long)result;
}

void LoadAverageShort12(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2) {
    weighted_sum_svector(v0, v1, p0, p1, v2, GPF12, GPL12);
}

long libgte_weighted_sum_svector0(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2) {
    return weighted_sum_svector(v0, v1, p0, p1, v2, GPF0, GPL0);
}

static long weighted_sum_bytes(unsigned char *v0, unsigned char *v1, long p0, long p1,
                               unsigned char *v2, int count) {
    unsigned int result;
    int i;

    mtc2(8, (unsigned int)p0);
    for (i = 0; i < count; i++) {
        mtc2(9 + i, BYTE(v0, i));
    }
    cop2(GPF0);
    result = mfc2(31);
    mtc2(8, (unsigned int)p1);
    for (i = 0; i < count; i++) {
        mtc2(9 + i, BYTE(v1, i));
    }
    cop2(GPL0);
    for (i = 0; i < count; i++) {
        BYTE(v2, i) = (unsigned char)srav((int)mfc2(25 + i), 12);
    }
    return (long)result;
}

long libgte_weighted_sum_2_bytes(unsigned char *v0, unsigned char *v1, long p0, long p1,
                                 unsigned char *v2) {
    return weighted_sum_bytes(v0, v1, p0, p1, v2, 2);
}

long libgte_weighted_sum_3_bytes(unsigned char *v0, unsigned char *v1, long p0, long p1,
                                 unsigned char *v2) {
    return weighted_sum_bytes(v0, v1, p0, p1, v2, 3);
}

/* The rotation registers from a MATRIX's first five words. */
static void set_rotation(MATRIX *m) {
    ctc2(0, WORD(m, 0));
    ctc2(1, WORD(m, 4));
    ctc2(2, WORD(m, 8));
    ctc2(3, WORD(m, 12));
    ctc2(4, WORD(m, 16));
}

/* RT * m1, column by column through V0 (MVMVA sf = 1): the five words of
 * the product's rotation part, word 16 being IR3 of the last column whole. */
static void multiply_columns(MATRIX *m0, MATRIX *m1, unsigned int *words) {
    unsigned int c0[3];
    unsigned int c1[3];

    set_rotation(m0);
    mtc2(0, HALF(m1, 0) | (WORD(m1, 4) & 0xFFFF0000u));
    mtc2(1, WORD(m1, 12));
    cop2(MVMVA_RT_V0);
    c0[0] = mfc2(9);
    c0[1] = mfc2(10);
    c0[2] = mfc2(11);
    mtc2(0, HALF(m1, 2) | WORD(m1, 8) << 16);
    mtc2(1, (unsigned int)SHALF(m1, 14));
    cop2(MVMVA_RT_V0);
    c1[0] = mfc2(9);
    c1[1] = mfc2(10);
    c1[2] = mfc2(11);
    mtc2(0, HALF(m1, 4) | (WORD(m1, 8) & 0xFFFF0000u));
    mtc2(1, WORD(m1, 16));
    cop2(MVMVA_RT_V0);
    words[0] = (c0[0] & 0xFFFF) | c1[0] << 16;
    words[3] = (c0[2] & 0xFFFF) | c1[2] << 16;
    words[1] = (mfc2(9) & 0xFFFF) | c0[1] << 16;
    words[2] = (c1[1] & 0xFFFF) | mfc2(10) << 16;
    words[4] = mfc2(11);
}

/* The product's words in the original's store order: 0, 12, 4, 8, 16. */
static void store_product(void *out, unsigned int *words) {
    WORD(out, 0) = words[0];
    WORD(out, 12) = words[3];
    WORD(out, 4) = words[1];
    WORD(out, 8) = words[2];
    WORD(out, 16) = words[4];
}

MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2) {
    unsigned int words[5];

    multiply_columns(m0, m1, words);
    store_product(m2, words);
    return m2;
}

/* m2 = m0 * m1, with m2's translation m0's rotation of m1's translation's
 * low halves plus m0's translation. */
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2) {
    unsigned int words[5];
    int t[3];

    multiply_columns(m0, m1, words);
    WORD(m2, 0) = words[0];
    WORD(m2, 12) = words[3];
    WORD(m2, 16) = words[4];
    mtc2(0, HALF(m1, 20) | WORD(m1, 24) << 16);
    mtc2(1, WORD(m1, 28));
    cop2(MVMVA_RT_V0);
    WORD(m2, 4) = words[1];
    WORD(m2, 8) = words[2];
    t[0] = (int)mfc2(25);
    t[1] = (int)mfc2(26);
    t[2] = (int)mfc2(27);
    t[0] += (int)WORD(m0, 20);
    t[1] += (int)WORD(m0, 24);
    t[2] += (int)WORD(m0, 28);
    WORD(m2, 20) = (unsigned int)t[0];
    WORD(m2, 24) = (unsigned int)t[1];
    WORD(m2, 28) = (unsigned int)t[2];
    return m2;
}

/* A long vector's component c as c >> 15 and c & 7FFFh, both with c's sign. */
static void split_component(int c, int *high, int *low) {
    unsigned int magnitude;

    if (c >= 0) {
        *high = c >> 15;
        *low = c & 0x7FFF;
    } else {
        magnitude = 0u - (unsigned int)c;
        *high = -((int)magnitude >> 15);
        *low = -((int)magnitude & 0x7FFF);
    }
}

/* RT * v0 for a long vector: the high parts (sf = 0) times 8 plus the low
 * parts (sf = 1). */
static void rotate_long_vector(void *v0, void *v1) {
    int high[3];
    int low[3];
    int product[3];
    int i;

    for (i = 0; i < 3; i++) {
        split_component((int)WORD(v0, i * 4), &high[i], &low[i]);
    }
    for (i = 0; i < 3; i++) {
        mtc2(9 + i, (unsigned int)high[i]);
    }
    cop2(MVMVA_RT_IR_SF0);
    for (i = 0; i < 3; i++) {
        product[i] = (int)mfc2(25 + i);
    }
    for (i = 0; i < 3; i++) {
        mtc2(9 + i, (unsigned int)low[i]);
    }
    cop2(MVMVA_RT_IR);
    for (i = 0; i < 3; i++) {
        WORD(v1, i * 4) = mfc2(25 + i) + ((unsigned int)product[i] << 3);
    }
}

VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1) {
    set_rotation(m);
    rotate_long_vector(v0, v1);
    return v1;
}

/* IR of RT * v0 (no translation) as a long vector. */
void libgte_rotate_svector(SVECTOR *v0, VECTOR *v1) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(MVMVA_RT_V0);
    swc2(9, v1, 0);
    swc2(10, v1, 4);
    swc2(11, v1, 8);
}

/* The rotation and translation registers onto the SDK's 20-matrix stack
 * (libgte_matrix_stack), or the overflow message (libgte_push_matrix_error,
 * libgte_pop_matrix_error) to the game's empty debug print. */
void PushMatrix(void) {
    int depth = libgte_matrix_stack_depth;
    char *top;
    int i;

    if (depth >= 0x280) {
        mode_empty_debug_print();
        return;
    }
    top = (char *)libgte_matrix_stack + depth;
    for (i = 0; i < 8; i++) {
        WORD(top, i * 4) = cfc2(i);
    }
    libgte_matrix_stack_depth = depth + 0x20;
}

void PopMatrix(void) {
    int depth = libgte_matrix_stack_depth;
    char *top;
    int i;

    if (depth <= 0) {
        mode_empty_debug_print();
        return;
    }
    depth -= 0x20;
    libgte_matrix_stack_depth = depth;
    top = (char *)libgte_matrix_stack + depth;
    for (i = 0; i < 8; i++) {
        ctc2(i, WORD(top, i * 4));
    }
}

/* m's nine elements times the scale factors selected by scale[0..8] from
 * v (>> 12), word by word: the low halves as halfwords, the last element's
 * result as a whole word over the pad. */
static MATRIX *scale_matrix(MATRIX *m, VECTOR *v, const int *scale) {
    int factor[3];
    unsigned int word;
    int i;

    factor[0] = (int)WORD(v, 0);
    factor[1] = (int)WORD(v, 4);
    factor[2] = (int)WORD(v, 8);
    for (i = 0; i < 4; i++) {
        word = WORD(m, i * 4);
        WORD(m, i * 4) = ((unsigned int)(mul(sign16(word), factor[scale[i * 2]]) >> 12) & 0xFFFF) |
                         (unsigned int)(mul((int)word >> 16, factor[scale[i * 2 + 1]]) >> 12) << 16;
    }
    WORD(m, 16) = (unsigned int)(mul(sign16(WORD(m, 16)), factor[scale[8]]) >> 12);
    return m;
}

/* Rows scaled by v's x, y, z. */
MATRIX *ScaleMatrixL(MATRIX *m, VECTOR *v) {
    static const int rows[9] = {0, 0, 0, 1, 1, 1, 2, 2, 2};

    return scale_matrix(m, v, rows);
}

/* Columns scaled by v's x, y, z. */
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v) {
    static const int columns[9] = {0, 1, 2, 0, 1, 2, 0, 1, 2};

    return scale_matrix(m, v, columns);
}

/* The rotation registers = RT * m1 (m0 loaded first); returns m0. */
MATRIX *SetMulMatrix(MATRIX *m0, MATRIX *m1) {
    unsigned int words[5];

    multiply_columns(m0, m1, words);
    ctc2(0, words[0]);
    ctc2(1, words[1]);
    ctc2(2, words[2]);
    ctc2(3, words[3]);
    ctc2(4, words[4]);
    return m0;
}

/* RT * v0 for a long vector, without loading a matrix (v1 is also the
 * original's v0, unused by the void prototype). */
void libgte_rotate_vector(VECTOR *v0, VECTOR *v1) { rotate_long_vector(v0, v1); }

/* m0 = m0 * m1. */
MATRIX *libgte_multiply_matrix_in_place(MATRIX *m0, MATRIX *m1) {
    unsigned int words[5];

    multiply_columns(m0, m1, words);
    store_product(m0, words);
    return m0;
}

/* m1 = m0 * m1. */
MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1) {
    unsigned int words[5];

    multiply_columns(m0, m1, words);
    store_product(m1, words);
    return m1;
}

/* v1 = m * v0 (MAC, no translation). */
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1) {
    set_rotation(m);
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(MVMVA_RT_V0);
    swc2(25, v1, 0);
    swc2(26, v1, 4);
    swc2(27, v1, 8);
    return v1;
}

/* v1 = m * v0 (IR, saturated). */
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1) {
    unsigned int x;
    unsigned int y;
    unsigned int z;

    set_rotation(m);
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(MVMVA_RT_V0);
    x = mfc2(9);
    y = mfc2(10);
    z = mfc2(11);
    HALF(v1, 0) = (unsigned short)x;
    HALF(v1, 2) = (unsigned short)y;
    HALF(v1, 4) = (unsigned short)z;
    return v1;
}

MATRIX *TransMatrix(MATRIX *m, VECTOR *v) {
    unsigned int x = WORD(v, 0);
    unsigned int y = WORD(v, 4);
    unsigned int z = WORD(v, 8);

    WORD(m, 20) = x;
    WORD(m, 24) = y;
    WORD(m, 28) = z;
    return m;
}

void SetRotMatrix(MATRIX *m) { set_rotation(m); }

void SetLightMatrix(MATRIX *m) {
    int i;

    for (i = 0; i < 5; i++) {
        ctc2(8 + i, WORD(m, i * 4));
    }
}

void SetColorMatrix(MATRIX *m) {
    int i;

    for (i = 0; i < 5; i++) {
        ctc2(16 + i, WORD(m, i * 4));
    }
}

void SetTransMatrix(MATRIX *m) {
    ctc2(5, WORD(m, 20));
    ctc2(6, WORD(m, 24));
    ctc2(7, WORD(m, 28));
}

/* Register loaders (gte_ldv0 and the like as functions). */
void libgte_load_v0(SVECTOR *v0) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
}

void libgte_load_v1(SVECTOR *v1) {
    lwc2(2, v1, 0);
    lwc2(3, v1, 4);
}

void libgte_load_v2(SVECTOR *v2) {
    lwc2(4, v2, 0);
    lwc2(5, v2, 4);
}

void libgte_load_v0_v1_v2(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2) {
    libgte_load_v0(v0);
    libgte_load_v1(v1);
    libgte_load_v2(v2);
}

void libgte_load_rgb_fifo(CVECTOR *c0, CVECTOR *c1, CVECTOR *c2) {
    lwc2(20, c0, 0);
    lwc2(21, c1, 0);
    lwc2(22, c2, 0);
}

void libgte_set_ir1_ir2_ir3(long ir1, long ir2, long ir3) {
    mtc2(9, (unsigned int)ir1);
    mtc2(10, (unsigned int)ir2);
    mtc2(11, (unsigned int)ir3);
}

void libgte_set_ir0(long ir0) { mtc2(8, (unsigned int)ir0); }

void libgte_set_sz1_sz2_sz3(long sz1, long sz2, long sz3) {
    mtc2(17, (unsigned int)sz1);
    mtc2(18, (unsigned int)sz2);
    mtc2(19, (unsigned int)sz3);
}

void libgte_set_sz0_sz1_sz2_sz3(long sz0, long sz1, long sz2, long sz3) {
    mtc2(16, (unsigned int)sz0);
    mtc2(17, (unsigned int)sz1);
    mtc2(18, (unsigned int)sz2);
    mtc2(19, (unsigned int)sz3);
}

void libgte_set_sxy0_sxy1_sxy2(long sxy0, long sxy1, long sxy2) {
    mtc2(12, (unsigned int)sxy0);
    mtc2(13, (unsigned int)sxy1);
    mtc2(14, (unsigned int)sxy2);
}

/* Whole words into RT11RT12, RT22RT23 and RT33. */
void libgte_set_rotation_diagonal(long d1, long d2, long d3) {
    ctc2(0, (unsigned int)d1);
    ctc2(2, (unsigned int)d2);
    ctc2(4, (unsigned int)d3);
}

void libgte_set_mac1_mac2_mac3(long mac1, long mac2, long mac3) {
    mtc2(25, (unsigned int)mac1);
    mtc2(26, (unsigned int)mac2);
    mtc2(27, (unsigned int)mac3);
}

void libgte_set_lzcs(long lzcs) { mtc2(30, (unsigned int)lzcs); }

void SetDQA(long dqa) { ctc2(27, (unsigned int)dqa); }

void SetDQB(long dqb) { ctc2(28, (unsigned int)dqb); }

void ReadGeomOffset(long *ofx, long *ofy) {
    int x = (int)cfc2(24) >> 16;
    int y = (int)cfc2(25) >> 16;

    *ofx = x;
    *ofy = y;
}

/* H as cfc2 reads it, sign extended. */
long ReadGeomScreen(void) { return (int)cfc2(26); }

void SetBackColor(long rbk, long gbk, long bbk) {
    ctc2(13, (unsigned int)rbk << 4);
    ctc2(14, (unsigned int)gbk << 4);
    ctc2(15, (unsigned int)bbk << 4);
}

void SetFarColor(long r, long g, long b) {
    ctc2(21, (unsigned int)r << 4);
    ctc2(22, (unsigned int)g << 4);
    ctc2(23, (unsigned int)b << 4);
}

void SetGeomOffset(long ofx, long ofy) {
    ctc2(24, (unsigned int)ofx << 16);
    ctc2(25, (unsigned int)ofy << 16);
}

void SetGeomScreen(long h) { ctc2(26, (unsigned int)h); }

/* LocalLight: v1 = LLM * v0 (IR, lm). */
void libgte_apply_light_matrix(SVECTOR *v0, VECTOR *v1) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(MVMVA_LLM_V0);
    swc2(9, v1, 0);
    swc2(10, v1, 4);
    swc2(11, v1, 8);
}

/* DpqColor: v1 = the colour v0 depth cued by p (DPCS). */
void libgte_depth_cue_color(CVECTOR *v0, long p, CVECTOR *v1) {
    lwc2(6, v0, 0);
    mtc2(8, (unsigned int)p);
    cop2(DPCS);
    swc2(22, v1, 0);
}

void NormalColor(SVECTOR *v0, CVECTOR *v1) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(NCS);
    swc2(22, v1, 0);
}

static void load_v0_v1_v2(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    lwc2(2, v1, 0);
    lwc2(3, v1, 4);
    lwc2(4, v2, 0);
    lwc2(5, v2, 4);
}

static void store_rgb_fifo(CVECTOR *c0, CVECTOR *c1, CVECTOR *c2) {
    swc2(20, c0, 0);
    swc2(21, c1, 0);
    swc2(22, c2, 0);
}

void NormalColor3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, CVECTOR *v4, CVECTOR *v5) {
    load_v0_v1_v2(v0, v1, v2);
    cop2(NCT);
    store_rgb_fifo(v3, v4, v5);
}

/* NormalColorDpq: NCDS of v0 in colour v1 with depth p. */
void libgte_normal_color_depth_cue(SVECTOR *v0, CVECTOR *v1, long p, CVECTOR *v2) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    lwc2(6, v1, 0);
    mtc2(8, (unsigned int)p);
    cop2(NCDS);
    swc2(22, v2, 0);
}

/* NormalColorDpq3: NCDT. */
void libgte_normal_color_depth_cue3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, long p,
                                    CVECTOR *v4, CVECTOR *v5, CVECTOR *v6) {
    load_v0_v1_v2(v0, v1, v2);
    lwc2(6, v3, 0);
    mtc2(8, (unsigned int)p);
    cop2(NCDT);
    store_rgb_fifo(v4, v5, v6);
}

void NormalColorCol(SVECTOR *v0, CVECTOR *v1, CVECTOR *v2) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    lwc2(6, v1, 0);
    cop2(NCCS);
    swc2(22, v2, 0);
}

void NormalColorCol3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, CVECTOR *v4, CVECTOR *v5,
                     CVECTOR *v6) {
    load_v0_v1_v2(v0, v1, v2);
    lwc2(6, v3, 0);
    cop2(NCCT);
    store_rgb_fifo(v4, v5, v6);
}

static void load_ir_vector(VECTOR *v) {
    lwc2(9, v, 0);
    lwc2(10, v, 4);
    lwc2(11, v, 8);
}

/* ColorDpq: CDP of the light vector v0 in colour v1 with depth p. */
void libgte_color_depth_cue(VECTOR *v0, CVECTOR *v1, long p, CVECTOR *v2) {
    load_ir_vector(v0);
    lwc2(6, v1, 0);
    mtc2(8, (unsigned int)p);
    cop2(CDP);
    swc2(22, v2, 0);
}

/* ColorCol: CC. */
void libgte_color_by_light_vector(VECTOR *v0, CVECTOR *v1, CVECTOR *v2) {
    load_ir_vector(v0);
    lwc2(6, v1, 0);
    cop2(CC);
    swc2(22, v2, 0);
}

/* AVSZ3/AVSZ4 of the depths already in SZ0-SZ3: OTZ. */
long libgte_average_stored_z3(void) {
    cop2(AVSZ3);
    return (long)mfc2(7);
}

long libgte_average_stored_z4(void) {
    cop2(AVSZ4);
    return (long)mfc2(7);
}

/* LightColor: v1 = BK + LCM * v0 (IR, lm). */
void libgte_apply_color_matrix(VECTOR *v0, VECTOR *v1) {
    load_ir_vector(v0);
    cop2(MVMVA_LCM_IR_BK);
    swc2(9, v1, 0);
    swc2(10, v1, 4);
    swc2(11, v1, 8);
}

/* DpqColorLight: DCPL. */
void libgte_depth_cue_light_color(VECTOR *v0, CVECTOR *v1, long p, CVECTOR *v2) {
    load_ir_vector(v0);
    lwc2(6, v1, 0);
    mtc2(8, (unsigned int)p);
    cop2(DCPL);
    swc2(22, v2, 0);
}

/* DpqColor3: DPCT of the three colours placed in the colour FIFO, with
 * RGBC (its CODE byte) loaded from the third. */
void libgte_depth_cue_color3(CVECTOR *v0, CVECTOR *v1, CVECTOR *v2, long p, CVECTOR *v3,
                             CVECTOR *v4, CVECTOR *v5) {
    lwc2(20, v0, 0);
    lwc2(21, v1, 0);
    lwc2(22, v2, 0);
    lwc2(6, v2, 0);
    mtc2(8, (unsigned int)p);
    cop2(DPCT);
    store_rgb_fifo(v3, v4, v5);
}

/* Intpl: INTPL of the vector v0 towards the far colour by p. */
void libgte_interpolate_far_color(VECTOR *v0, long p, CVECTOR *v1) {
    load_ir_vector(v0);
    mtc2(8, (unsigned int)p);
    cop2(INTPL);
    swc2(22, v1, 0);
}

/* Square12 and Square0: v1 = v0's components squared (MAC). */
VECTOR *libgte_square_vector12(VECTOR *v0, VECTOR *v1) {
    load_ir_vector(v0);
    cop2(SQR12);
    swc2(25, v1, 0);
    swc2(26, v1, 4);
    swc2(27, v1, 8);
    return v1;
}

void Square0(VECTOR *v0, VECTOR *v1) {
    load_ir_vector(v0);
    cop2(SQR0);
    swc2(25, v1, 0);
    swc2(26, v1, 4);
    swc2(27, v1, 8);
}

/* AverageZ3 and AverageZ4 of the given depths. */
long libgte_average_z3(long sz0, long sz1, long sz2) {
    mtc2(17, (unsigned int)sz0);
    mtc2(18, (unsigned int)sz1);
    mtc2(19, (unsigned int)sz2);
    cop2(AVSZ3);
    return (long)mfc2(7);
}

long libgte_average_z4(long sz0, long sz1, long sz2, long sz3) {
    mtc2(16, (unsigned int)sz0);
    mtc2(17, (unsigned int)sz1);
    mtc2(18, (unsigned int)sz2);
    mtc2(19, (unsigned int)sz3);
    cop2(AVSZ4);
    return (long)mfc2(7);
}

/* v2 = v0 x v1 through OP, the rotation diagonal saved and restored. */
static void outer_product(VECTOR *v0, VECTOR *v1, VECTOR *v2, unsigned int op) {
    unsigned int d0 = cfc2(0), d1 = cfc2(2), d2 = cfc2(4);

    ctc2(0, WORD(v0, 0));
    ctc2(2, WORD(v0, 4));
    ctc2(4, WORD(v0, 8));
    lwc2(11, v1, 8);
    lwc2(9, v1, 0);
    lwc2(10, v1, 4);
    cop2(op);
    swc2(25, v2, 0);
    swc2(26, v2, 4);
    swc2(27, v2, 8);
    ctc2(0, d0);
    ctc2(2, d1);
    ctc2(4, d2);
}

void OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2) { outer_product(v0, v1, v2, OP12); }

void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2) { outer_product(v0, v1, v2, OP0); }

/* Lzc: LZCR of a. */
long libgte_count_leading_zeros(long a) {
    mtc2(30, (unsigned int)a);
    return (long)mfc2(31);
}

/* v1 = RT * v0 + TR saturated, with IR3 stored as a word over v1's pad;
 * FLAG to *flag (also the original's v0, unused by the void prototype). */
void RotTransSV(SVECTOR *v0, SVECTOR *v1, long *flag) {
    unsigned int x;
    unsigned int y;

    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(MVMVA_RT_V0_TR);
    x = mfc2(9);
    y = mfc2(10);
    swc2(11, v1, 4);
    HALF(v1, 0) = (unsigned short)x;
    HALF(v1, 2) = (unsigned short)y;
    *flag = (long)cfc2(31);
}

/* Short vectors squared: x and y from halfwords, z from v0's second word,
 * the result as halfwords and a word over the pad, or as a long vector. */
static void load_square_svector(SVECTOR *v0, unsigned int sqr) {
    mtc2(9, (unsigned int)SHALF(v0, 0));
    mtc2(10, (unsigned int)SHALF(v0, 2));
    lwc2(11, v0, 4);
    cop2(sqr);
}

static SVECTOR *square_svector(SVECTOR *v0, SVECTOR *v1, unsigned int sqr) {
    unsigned int x;
    unsigned int y;

    load_square_svector(v0, sqr);
    x = mfc2(9);
    y = mfc2(10);
    swc2(11, v1, 4);
    HALF(v1, 0) = (unsigned short)x;
    HALF(v1, 2) = (unsigned short)y;
    return v1;
}

SVECTOR *libgte_square_svector12(SVECTOR *v0, SVECTOR *v1) { return square_svector(v0, v1, SQR12); }

SVECTOR *libgte_square_svector0(SVECTOR *v0, SVECTOR *v1) { return square_svector(v0, v1, SQR0); }

static VECTOR *square_svector_to_vector(SVECTOR *v0, VECTOR *v1, unsigned int sqr) {
    load_square_svector(v0, sqr);
    swc2(9, v1, 0);
    swc2(10, v1, 4);
    swc2(11, v1, 8);
    return v1;
}

VECTOR *libgte_square_svector12_to_vector(SVECTOR *v0, VECTOR *v1) {
    return square_svector_to_vector(v0, v1, SQR12);
}

VECTOR *libgte_square_svector0_to_vector(SVECTOR *v0, VECTOR *v1) {
    return square_svector_to_vector(v0, v1, SQR0);
}

/* The projections return SZ3 / 4 (sra). */
long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag) {
    unsigned int f;
    int z;

    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(RTPS);
    swc2(14, sxy, 0);
    swc2(8, p, 0);
    f = cfc2(31);
    z = (int)mfc2(19);
    *flag = (long)f;
    return z >> 2;
}

long RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2,
                   long *p, long *flag) {
    unsigned int f;
    int z;

    load_v0_v1_v2(v0, v1, v2);
    cop2(RTPT);
    swc2(12, sxy0, 0);
    swc2(13, sxy1, 0);
    swc2(14, sxy2, 0);
    swc2(8, p, 0);
    f = cfc2(31);
    z = (int)mfc2(19);
    *flag = (long)f;
    return z >> 2;
}

/* v1 = RT * v0 + TR (MAC); FLAG to *flag (also the original's v0). */
void RotTrans(SVECTOR *v0, VECTOR *v1, long *flag) {
    lwc2(0, v0, 0);
    lwc2(1, v0, 4);
    cop2(MVMVA_RT_V0_TR);
    swc2(25, v1, 0);
    swc2(26, v1, 4);
    swc2(27, v1, 8);
    *flag = (long)cfc2(31);
}

long NormalClip(long sxy0, long sxy1, long sxy2) {
    mtc2(12, (unsigned int)sxy0);
    mtc2(14, (unsigned int)sxy2);
    mtc2(13, (unsigned int)sxy1);
    cop2(NCLIP);
    return (long)mfc2(24);
}

/* RTPT of v0-v2, then RTPS of v3; *flag is the OR of both FLAGs. */
static unsigned int project_quad(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0,
                                 long *sxy1, long *sxy2, long *sxy3, long *p) {
    unsigned int f;

    load_v0_v1_v2(v0, v1, v2);
    cop2(RTPT);
    swc2(12, sxy0, 0);
    swc2(13, sxy1, 0);
    swc2(14, sxy2, 0);
    f = cfc2(31);
    lwc2(0, v3, 0);
    lwc2(1, v3, 4);
    cop2(RTPS);
    swc2(14, sxy3, 0);
    f |= cfc2(31);
    swc2(8, p, 0);
    return f;
}

long RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1,
                   long *sxy2, long *sxy3, long *p, long *flag) {
    unsigned int f = project_quad(v0, v1, v2, v3, sxy0, sxy1, sxy2, sxy3, p);
    int z = (int)mfc2(19);

    *flag = (long)f;
    return z >> 2;
}

/* As RotTransPers4, returning the AVSZ4 depth (OTZ). */
long RotAverage4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1,
                 long *sxy2, long *sxy3, long *p, long *flag) {
    *flag = (long)project_quad(v0, v1, v2, v3, sxy0, sxy1, sxy2, sxy3, p);
    cop2(AVSZ4);
    return (long)mfc2(7);
}

/* RTPT of v0-v2 (FLAG to *flag) and NCLIP; a front-facing quad (NCLIP > 0)
 * stores its points, projects v3, ORs the FLAGs and stores the AVSZ4 depth.
 * Returns the NCLIP value either way. */
long libgte_project_front_quad(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0,
                               long *sxy1, long *sxy2, long *sxy3, long *p, long *otz, long *flag) {
    unsigned int f;
    int clip;

    load_v0_v1_v2(v0, v1, v2);
    cop2(RTPT);
    f = cfc2(31);
    *flag = (long)f;
    cop2(NCLIP);
    clip = (int)mfc2(24);
    if (clip <= 0) {
        return clip;
    }
    swc2(12, sxy0, 0);
    swc2(13, sxy1, 0);
    swc2(14, sxy2, 0);
    lwc2(0, v3, 0);
    lwc2(1, v3, 4);
    cop2(RTPS);
    swc2(14, sxy3, 0);
    f |= cfc2(31);
    swc2(8, p, 0);
    *flag = (long)f;
    cop2(AVSZ4);
    *otz = (long)mfc2(7);
    return clip;
}

/* out = m transposed, through the original's word and halfword stores (so
 * that out == m transposes in place the same way). */
void libgte_transpose_matrix(MATRIX *m, MATRIX *out) {
    unsigned int a = WORD(m, 0);
    unsigned int b = WORD(m, 4);
    unsigned int c;
    unsigned int d;
    unsigned short e;

    WORD(out, 4) = a;
    WORD(out, 0) = b;
    HALF(out, 0) = (unsigned short)a;
    c = WORD(m, 8);
    d = WORD(m, 12);
    WORD(out, 12) = c;
    WORD(out, 8) = d;
    HALF(out, 12) = (unsigned short)b;
    HALF(out, 8) = (unsigned short)c;
    e = HALF(m, 16);
    HALF(out, 4) = (unsigned short)d;
    HALF(out, 16) = e;
}

/* An angle's sine and cosine, the sine of a negative angle negated. */
static void sin_cos(int angle, int *sine, int *cosine) {
    unsigned int pair = sin_cos_pair(angle);

    *sine = angle < 0 ? -sign16(pair) : sign16(pair);
    *cosine = (int)pair >> 16;
}

static int mul12(int a, int b) { return mul(a, b) >> 12; }

/* Rotation matrices from rcossin_tbl, each product >> 12 as the original
 * forms them. */
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m) {
    int sx, cx, sy, cy, sz, cz;
    int sysx, cysx;

    sin_cos(SHALF(r, 0), &sx, &cx);
    sin_cos(SHALF(r, 2), &sy, &cy);
    sin_cos(SHALF(r, 4), &sz, &cz);
    SHALF(m, 10) = (short)-sx;
    SHALF(m, 4) = (short)mul12(sy, cx);
    SHALF(m, 16) = (short)mul12(cy, cx);
    SHALF(m, 6) = (short)mul12(sz, cx);
    SHALF(m, 8) = (short)mul12(cz, cx);
    sysx = mul12(sy, sx);
    SHALF(m, 0) = (short)(mul12(cy, cz) + mul12(sysx, sz));
    SHALF(m, 2) = (short)(-mul12(cy, sz) + mul12(sysx, cz));
    cysx = mul12(cy, sx);
    SHALF(m, 14) = (short)(mul12(sy, sz) + mul12(cysx, cz));
    SHALF(m, 12) = (short)(-mul12(sy, cz) + mul12(cysx, sz));
    return m;
}

MATRIX *RotMatrix(SVECTOR *r, MATRIX *m) {
    int sx, cx, sy, cy, sz, cz;
    int sxsy, sycx;

    sin_cos(SHALF(r, 0), &sx, &cx);
    sin_cos(SHALF(r, 2), &sy, &cy);
    sin_cos(SHALF(r, 4), &sz, &cz);
    SHALF(m, 12) = (short)-sy;
    SHALF(m, 14) = (short)mul12(sx, cy);
    SHALF(m, 16) = (short)mul12(cx, cy);
    SHALF(m, 0) = (short)mul12(cy, cz);
    SHALF(m, 6) = (short)mul12(sz, cy);
    sxsy = mul12(sx, sy);
    SHALF(m, 2) = (short)(mul12(sxsy, cz) - mul12(sz, cx));
    SHALF(m, 8) = (short)(mul12(sxsy, sz) + mul12(cx, cz));
    sycx = mul12(sy, cx);
    SHALF(m, 4) = (short)(mul12(sycx, cz) + mul12(sx, sz));
    SHALF(m, 10) = (short)(mul12(sycx, sz) - mul12(sx, cz));
    return m;
}

/* m = R * m for a rotation about one axis: the rows (or row pairs) a and b,
 * as halfword offsets, become (c*a - s*b) >> 12 and (s*a + c*b) >> 12, the
 * products summed before the shift. */
static MATRIX *rotate_rows(MATRIX *m, int s, int c, int a, int b) {
    int ra[3];
    int rb[3];
    int i;

    for (i = 0; i < 3; i++) {
        ra[i] = SHALF(m, a + i * 2);
        rb[i] = SHALF(m, b + i * 2);
    }
    for (i = 0; i < 3; i++) {
        SHALF(m, a + i * 2) =
            (short)((int)((unsigned int)mul(c, ra[i]) - (unsigned int)mul(s, rb[i])) >> 12);
    }
    for (i = 0; i < 3; i++) {
        SHALF(m, b + i * 2) =
            (short)((int)((unsigned int)mul(s, ra[i]) + (unsigned int)mul(c, rb[i])) >> 12);
    }
    return m;
}

MATRIX *RotMatrixX(long r, MATRIX *m) {
    int s, c;

    sin_cos((int)r, &s, &c);
    return rotate_rows(m, s, c, 6, 12);
}

/* Y's rows 0 and 2 take -sin. */
MATRIX *RotMatrixY(long r, MATRIX *m) {
    int s, c;

    sin_cos((int)r, &s, &c);
    return rotate_rows(m, -s, c, 0, 12);
}

MATRIX *RotMatrixZ(long r, MATRIX *m) {
    int s, c;

    sin_cos((int)r, &s, &c);
    return rotate_rows(m, s, c, 0, 6);
}

/* The angle of (x, y), 4096 = one turn, from the arctangent table of the
 * smaller over the larger magnitude in 1024 steps. */
long ratan2(long y, long x) {
    int ny = 0;
    int nx = 0;
    int a = (int)y;
    int b = (int)x;
    int angle;

    if (b < 0) {
        nx = 1;
        b = (int)(0u - (unsigned int)b);
    }
    if (a < 0) {
        ny = 1;
        a = (int)(0u - (unsigned int)a);
    }
    if (b == 0 && a == 0) {
        return 0;
    }
    if (a < b) {
        if (a & 0x7FE00000) {
            angle = table_half(libgte_arctangent_table, divide(a, b >> 10));
        } else {
            angle = table_half(libgte_arctangent_table, divide(sllv(a, 10), b));
        }
    } else {
        if (b & 0x7FE00000) {
            angle = table_half(libgte_arctangent_table, divide(b, a >> 10));
        } else {
            angle = table_half(libgte_arctangent_table, divide(sllv(b, 10), a));
        }
        angle = 0x400 - angle;
    }
    if (nx) {
        angle = 0x800 - angle;
    }
    if (ny) {
        angle = -angle;
    }
    return angle;
}

/* The original copies its exception-handler patch (libgte_patch_code up to
 * VSync) into the BIOS handler, between EnterCriticalSection and
 * ExitCriticalSection with a FlushCache, saving its return address in
 * libgte_patch_gte_return. The patch makes the handler save the GTE-related
 * state around interrupts arriving during a GTE instruction; the port's GTE
 * instructions are never interrupted and there is no BIOS handler, so it
 * patches nothing. */
void _patch_gte(void) {}

/* SetFogNearFar: DQA and DQB for fog from a to b at screen distance h,
 * left unchanged when b - a < 100. */
void SetFogNearFar(long a, long b, long h) {
    int range = (int)b - (int)a;
    int dqa;
    int dqb;

    if (range < 100) {
        return;
    }
    dqa = divide(mul(-(int)a, (int)b), range);
    dqb = divide(sllv((int)b, 12), range);
    dqa = divide(sllv(dqa, 8), (int)h);
    if (dqa < -0x8000) {
        dqa = -0x8000;
    }
    if (dqa > 0x7FFF) {
        dqa = 0x7FFF;
    }
    SetDQA(dqa);
    SetDQB(sllv(dqb, 12));
}

/* The handwritten rotation (gpu_build_rotation_matrix.s): Rx * Ry * Rz of
 * the three masked angles, only the nine rotation halfwords stored. */
MATRIX *gpu_build_rotation_matrix(SVECTOR *angles, MATRIX *m) {
    unsigned int xy = WORD(angles, 0);
    unsigned int x = xy & 0xFFF;
    unsigned int y = xy >> 16 & 0xFFF;
    unsigned int z = HALF(angles, 4) & 0xFFF;
    unsigned int ypair = WORD(rcossin_tbl, y * 4);
    unsigned int zpair = WORD(rcossin_tbl, z * 4);
    unsigned int xpair = WORD(rcossin_tbl, x * 4);
    int sx = sign16(xpair), cx = (int)xpair >> 16;
    int sy = sign16(ypair), cy = (int)ypair >> 16;
    int sz = sign16(zpair), cz = (int)zpair >> 16;
    int a = mul12(cz, -sy);
    int b = mul12(sz, -sy);

    SHALF(m, 0) = (short)mul12(cz, cy);
    SHALF(m, 6) = (short)(mul12(sz, cx) - mul12(a, sx));
    SHALF(m, 12) = (short)(mul12(a, cx) + mul12(sz, sx));
    SHALF(m, 2) = (short)(-mul(sz, cy) >> 12);
    SHALF(m, 8) = (short)(mul12(cz, cx) + mul12(b, sx));
    SHALF(m, 14) = (short)(mul12(cz, sx) - mul12(b, cx));
    SHALF(m, 4) = (short)sy;
    SHALF(m, 10) = (short)(-mul(cy, sx) >> 12);
    SHALF(m, 16) = (short)mul12(cy, cx);
    return m;
}

/* Sine and cosine of a 12-bit angle (4096 = 1.0). */
int gpu_get_sin(int angle) { return rcossin_tbl[angle & 0xFFF][0]; }

int gpu_get_cos(int angle) { return rcossin_tbl[angle & 0xFFF][1]; }
