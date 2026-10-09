#ifndef FIELD_FIELD_CAMERA_H
#define FIELD_FIELD_CAMERA_H

/* The field camera: its initial state, look-at and follow helpers, the angle
 * steps and octants, and the matrix utilities of field.c. The camera state
 * itself is the view D_800AF880 (field.h). */

#include "common.h"
#include "psyq/libgte.h"
#include "field.h"

extern MATRIX D_800AF85C;  /* the composed view, kept across frames */
extern MATRIX D_800AFC30;  /* sprite view rotation matrix */
extern MATRIX D_800B00E8;  /* instance view: the rotation with its translation */
extern s32 D_800B00B4;     /* camera pitch */
extern s32 D_800ADB94;     /* camera distance */
extern s32 D_800ADBA8;     /* 1 while the target goal is held at the walkable edge */
extern s32 D_800ADBAC;     /* camera frames settling */
extern s32 D_800ADBB0;     /* camera frames releasing */
extern s32 D_800ADC18;     /* camera cut: turns jump to their goal, pad input is dropped */
extern u8 D_800ADC1C[8];   /* octant bits */

void func_8007254C(void);                                /* the camera's initial state */
void func_800722F4(void);                                /* compose the view, reload the scaled world */
void func_80073750(MATRIX *view, VECTOR *eye, VECTOR *target, VECTOR *up); /* look-at matrix */
void func_80073684(VECTOR *point, VECTOR *center);       /* rotate about `center` by the heading */
void func_800723E4(DVECTOR *a, DVECTOR *b, DVECTOR *out); /* intersection of two lines */
s32 func_8007CD80(VECTOR *point, SVECTOR *edge, DVECTOR *segment); /* walk the top layer toward `point` */
s32 func_80073930(s32 angle, s32 goal, s32 step);        /* turn toward `goal` by `step` */
s32 func_80073988(s32 angle, s32 goal, s32 step);        /* the same, or jump there on a cut */
s32 func_8009A514(void);                                 /* camera octant (0..7) */
void func_80072254(s32 index);                           /* rebuild descriptor `index`'s matrix */

/* Matrix utilities. */
void func_80070594(MATRIX *m);               /* identity rotation, zero translation */
void func_80072140(MATRIX *m);               /* clear the translation */
void func_80074038(MATRIX *to, MATRIX *from); /* copy rotation and translation */
void func_80074078(MATRIX *to, MATRIX *from); /* copy the translation */
void func_8007409C(MATRIX *to, MATRIX *from); /* copy the rotation */
void func_80077844(MATRIX *m, s32 m00, s32 m01, s32 m02, s32 m10, s32 m11, s32 m12, s32 m20, s32 m21,
                   s32 m22);                  /* set the nine rotation elements */

#endif
