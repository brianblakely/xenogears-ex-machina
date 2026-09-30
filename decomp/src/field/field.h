#ifndef FIELD_FIELD_H
#define FIELD_FIELD_H

#include "common.h"

/* GTE vector/matrix layouts (PsyQ libgte). */
typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* One of the three field light/lookup slots at 800b06a4. */
typedef struct {
    s16 a;
    s16 b;
    s16 c;
} FieldSlot6;

/* Resident services. */
extern void func_80028A60(s32);
extern void func_80029EB0(s32 file, void *ring, s32, s32, s32, s32, s32, s32, s32, s32);
extern void *func_8002A260(s32 sectors, s32);
extern void func_800320E8(void *);
extern void func_80032EB4(s32 index, void *destination);
extern void func_8003F738(SVECTOR *angles, MATRIX *m); /* RotMatrix */
extern void func_800445D0(s32);

/* Field overlay. */
extern void func_80078C5C(void);

/* Resident state. */
extern s32 D_8004F34C; /* current map */

/* Field state. */
extern s32 D_800ADB0C;
extern s32 D_800ADB60; /* field stream running */
extern void *D_800ADC14; /* field stream ring */
extern FieldSlot6 D_800B06A4[3];

#endif
