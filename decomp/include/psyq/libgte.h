#ifndef PSYQ_LIBGTE_H
#define PSYQ_LIBGTE_H

#include "common.h"

/* PsyQ libgte geometry types and the resident's named GTE routines. */
typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 vx, vy;
} DVECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

void InitGeom(void);
void SetGeomOffset(s32 x, s32 y);
void SetBackColor(s32 r, s32 g, s32 b);
void PushMatrix(void);
void PopMatrix(void);
MATRIX *CompMatrix(MATRIX *a, MATRIX *b, MATRIX *out);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *in, VECTOR *out);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *scale);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
s32 RotTransPers(SVECTOR *v, s32 *sxy, s32 *p, s32 *flag);
s32 ratan2(s32 y, s32 x);
s32 SquareRoot0(s32 value);

#endif
