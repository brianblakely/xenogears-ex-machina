#ifndef OVL2143_PARTICLES_H
#define OVL2143_PARTICLES_H

/* Particles: the effect sprites (battle/effect.h) of the module's pool, and
 * the actors' colour fades, ribbons traced by two points of a node and
 * emitted as effect sprites. */

#include "common.h"
#include "psyq/libgte.h"
#include "battle/effect.h"

SpritePool *func_801E0064(SpritePool *pool, s32 capacity);
void func_801E00DC(SpritePool *pool);
void func_801E011C(SpritePool *pool);
EffectSprite *func_801E0248(SpritePool *pool, s16 semi_trans);
s32 func_801E0354(SpritePool *pool, EffectSprite *particle);
void func_801E0398(SpritePool *pool, MATRIX *m, s32 steps, u32 *ot, s32 buffer);
void func_801E0844(ColorFade *fade, s32 unused);

#endif
