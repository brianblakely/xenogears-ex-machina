#ifndef FIELD_FIELD_EFFECT_H
#define FIELD_FIELD_EFFECT_H

#include "field.h"

/* Particle effects: 64 slots (800b14b0 states, 800b0108 owners), each a
 * copy of the eight template emitters at 800b02cc with their particles. */

void func_800A92AC(s32 slot);
void func_800A9F18(Record78 *emitter, Particle *particle, MATRIX *view);
void func_800AA6B4(Record78 *emitter, Particle *particle, s32 *spawned);

#endif
