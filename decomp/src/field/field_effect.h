#ifndef FIELD_FIELD_EFFECT_H
#define FIELD_FIELD_EFFECT_H

#include "field.h"

/* Particle effects: 64 slots (800b14b0 states, 800b0108 owners), each a
 * copy of the eight template emitters at 800b02cc with their particles. */

extern s32 D_80050100; /* resident: depth shift */

extern s16 D_800B00B4; /* camera elevation */

void func_800495DC(SVECTOR *v, VECTOR *out);  /* resident: rotate by the GTE matrix */
void func_8004998C(VECTOR *v, VECTOR *out);   /* resident: rotate a long vector */
MATRIX *func_8004ABBC(SVECTOR *angles, MATRIX *m); /* resident: rotation matrix */
void func_801E72CC(MATRIX *m, MATRIX *work, s32 a, s32 b);
s32 func_800A9B1C(s32 value, s32 delta);
void func_800A9B54(Particle *particle, MATRIX *view, s16 angle, s32 depth_mode, VECTOR *scale, s32 mode);

void func_800A92AC(s32 slot);
void func_800A9F18(Record78 *emitter, Particle *particle, MATRIX *view);
void func_800AA6B4(Record78 *emitter, Particle *particle, s32 *spawned);

#endif
