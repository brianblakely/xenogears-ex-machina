#ifndef OVL2143_SURFACE_H
#define OVL2143_SURFACE_H

/* An actor's records24: surfaces of rings of points around a strand, lit
 * and drawn as gouraud textured triangles. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* An entry of a records24 table (0x10 bytes). */
typedef struct {
    s16 h0, h2, h4, h6, h8, hA, hC, hE;
} Record24Entry;

/* A point of a records24 strand (0x18 bytes): the length of its segment to
 * the next point (0 ends the strand), a sag added to that segment, its
 * position and the normal accumulated from its triangles. */
typedef struct {
    s16 length;
    s16 sag;
    s16 pos[3];
    u16 normal_count;       /* +a */
    s32 normal[3];          /* +c */
} RingPoint;

/* Two triangles' textured primitives (one per buffer) and their vertex
 * indices (0x58 bytes). */
typedef struct {
    s16 index[3];
    u8 pad6[2];
    POLY_GT3 prim[2];
} RingPoly;

/* An actor's 0x24-byte record (records24): a surface of rings of points. */
typedef struct {
    u16 h0;
    u8 pad2[2];
    s16 rings;              /* +4 */
    s16 polys;              /* +6: twice the rings' first point counts */
    s16 points;             /* +8 */
    s16 entry_count;        /* +a */
    u8 b[6];                /* +c */
    u8 pad12[2];
    SVECTOR *centres;       /* +14: a centre per ring */
    Record24Entry *block18; /* +18 */
    RingPoint **block1C;    /* +1c: each ring's first point */
    RingPoly *block20;      /* +20 */
} Record24;

void func_801E1A14(Record24 *record, u16 *table, s32 angle_base, s32 scale, s16 ox, s16 oy, s16 oz,
                   s32 count, s16 tx, s16 ty, s16 u_span, s16 v_span, s16 clut_x, s16 clut_y, u8 b0,
                   u8 b1, u8 b2, u8 b3, u8 b4, u8 b5);
void func_801E22F8(Record24 *record, SVECTOR *light, MATRIX *m, u32 *ot, s32 buffer, s32 scale,
                   s16 floor);
void func_801E3438(Record24 *record);

#endif
