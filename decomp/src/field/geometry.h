#ifndef FIELD_GEOMETRY_H
#define FIELD_GEOMETRY_H

#include "common.h"

/* A 16.16 fixed-point coordinate, also addressed by its whole part. */
typedef union {
    s32 value;
    struct {
        u16 fraction;
        s16 whole;
    } s;
} Fixed;

typedef struct {
    Fixed vx;
    Fixed vy;
    Fixed vz;
    s32 pad;
} VECTOR;

typedef struct {
    s16 vx;
    s16 vy;
    s16 vz;
    s16 pad;
} SVECTOR;

/* A plain 32-bit vector (GTE long vector). */
typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} LVECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* Resident GTE helpers (PsyQ libgte). */
void func_8004960C(void);                                  /* push matrix */
void func_800496AC(void);                                  /* pop matrix */
MATRIX *func_8003F738(SVECTOR *angles, MATRIX *m);         /* rotation */
LVECTOR *func_8004947C(MATRIX *m, LVECTOR *v, LVECTOR *out); /* apply */
s32 func_8004B32C(s32 y, s32 x);                           /* atan2 */

#endif
