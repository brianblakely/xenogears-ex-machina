#ifndef PSYQ_LIBGTE_H
#define PSYQ_LIBGTE_H

/* PsyQ libgte. */
typedef struct {
    long vx, vy, vz, pad;
} VECTOR;

typedef struct {
    short vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    short vx, vy;
} DVECTOR;

typedef struct {
    short m[3][3];
    long t[3];
} MATRIX;

#define setVector(v, _x, _y, _z) (v)->vx = _x, (v)->vy = _y, (v)->vz = _z

void InitGeom(void);
void SetGeomOffset(long ofx, long ofy);
void SetGeomScreen(long h);
void SetBackColor(long rbk, long gbk, long bbk);
void PushMatrix(void);
void PopMatrix(void);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag);
long ratan2(long y, long x);
long SquareRoot0(long a);

#endif
