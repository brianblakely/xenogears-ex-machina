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

void InitGeom(void);
void SetGeomOffset(long ofx, long ofy);
void SetGeomScreen(long h);
long ratan2(long y, long x);
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);

#endif
