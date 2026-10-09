#ifndef PSYQ_LIBGTE_H
#define PSYQ_LIBGTE_H

#include "psyq/types.h"

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
    u_char r, g, b, cd;
} CVECTOR;

typedef struct {
    short m[3][3];
    long t[3];
} MATRIX;

#define setVector(v, _x, _y, _z) (v)->vx = _x, (v)->vy = _y, (v)->vz = _z
#define copyVector(v0, v1) (v0)->vx = (v1)->vx, (v0)->vy = (v1)->vy, (v0)->vz = (v1)->vz

void InitGeom(void);
void SetGeomOffset(long ofx, long ofy);
void SetGeomScreen(long h);
void SetBackColor(long rbk, long gbk, long bbk);
void SetFogNearFar(long a, long b, long h);
void SetColorMatrix(MATRIX *m);
void SetLightMatrix(MATRIX *m);
void PushMatrix(void);
void PopMatrix(void);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *RotMatrixZ(long r, MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
MATRIX *SetMulMatrix(MATRIX *m0, MATRIX *m1);
void ReadGeomOffset(long *ofx, long *ofy);
long ReadGeomScreen(void);
MATRIX *ScaleMatrixL(MATRIX *m, VECTOR *v);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void RotTransSV(SVECTOR *v0, SVECTOR *v1, long *flag);
long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag);
long RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2,
                   long *p, long *flag);
long RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1,
                   long *sxy2, long *sxy3, long *p, long *flag);
long RotAverage4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1,
                 long *sxy2, long *sxy3, long *p, long *flag);
long VectorNormalS(VECTOR *v0, SVECTOR *v1);
void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
long ratan2(long y, long x);
long SquareRoot0(long a);
void NormalColor(SVECTOR *v0, CVECTOR *v1);
void NormalColor3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, CVECTOR *v4, CVECTOR *v5);
void NormalColorCol(SVECTOR *v0, CVECTOR *v1, CVECTOR *v2);
void NormalColorCol3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, CVECTOR *v4, CVECTOR *v5,
                     CVECTOR *v6);
void VectorNormalSS(SVECTOR *v0, SVECTOR *v1);
long VectorNormal(VECTOR *v0, VECTOR *v1);
MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1);

/* Members of libgte the symbol file does not name yet, by their signatures
 * and their callers' uses (the SDK function where the callers name it). */
void func_80048E94(MATRIX *m, MATRIX *out);     /* orthonormal rows from two outer products */
void func_8004901C(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2); /* LoadAverageShort12 */
void func_800495DC(SVECTOR *v0, VECTOR *v1);    /* rotate by the current GTE matrix */
void func_8004998C(VECTOR *v0, VECTOR *v1);     /* rotate a long vector by it */
MATRIX *func_80049ACC(MATRIX *m0, MATRIX *m1);  /* m0 = m0 * m1 */
void func_8004A10C(long r, long g, long b);     /* the far colour */
void func_8004A414(VECTOR *v0, VECTOR *v1);     /* Square0 */
void func_8004A480(VECTOR *v0, VECTOR *v1, VECTOR *v2); /* OuterProduct12 */
void func_8004A6DC(SVECTOR *v0, VECTOR *v1, long *flag); /* RotTrans */
long func_8004A70C(long sxy0, long sxy1, long sxy2); /* NormalClip */
long func_8004A83C(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1,
                   long *sxy2, long *sxy3, long *p, long *otz, long *flag);
void func_8004A8EC(MATRIX *m, MATRIX *out);
MATRIX *func_8004A92C(SVECTOR *r, MATRIX *m);   /* RotMatrixYXZ */
MATRIX *func_8004ABBC(SVECTOR *r, MATRIX *m);   /* RotMatrix */
MATRIX *func_8004AE4C(long r, MATRIX *m);       /* RotMatrixX */
MATRIX *func_8004AFEC(long r, MATRIX *m);       /* RotMatrixY */

#endif
