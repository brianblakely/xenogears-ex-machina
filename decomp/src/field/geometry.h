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

typedef struct {
    s16 vx;
    s16 vy;
} DVECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* Resident GTE helpers (PsyQ libgte). */
void func_8004960C(void);                                  /* push matrix */
void func_800496AC(void);                                  /* pop matrix */
MATRIX *func_8003F738(SVECTOR *angles, MATRIX *m);         /* rotation */
VECTOR *func_8004947C(MATRIX *m, VECTOR *v, VECTOR *out); /* apply */
MATRIX *func_8004931C(MATRIX *a, MATRIX *b, MATRIX *out); /* compose */
void func_80049EFC(MATRIX *m);                             /* set rotation */
void func_80049F8C(MATRIX *m);                             /* set translation */
s32 func_8004A64C(SVECTOR *v, DVECTOR *xy, s32 *p, s32 *flag); /* project */
s32 func_8004B32C(s32 y, s32 x);                           /* atan2 */
s32 func_8003F8B0(s32 angle);                              /* trig */
s32 func_8003F8CC(s32 angle);                              /* trig */

#endif
