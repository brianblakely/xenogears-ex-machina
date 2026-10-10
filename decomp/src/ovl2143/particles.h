#ifndef OVL2143_PARTICLES_H
#define OVL2143_PARTICLES_H

/* Particles: the effect sprites (battle/effect.h) of the module's pool, and
 * the actors' colour fades, ribbons traced by two points of a node and
 * emitted as effect sprites. */

#include "common.h"
#include "psyq/libgte.h"
#include "battle/effect.h"

SpritePool *gear_model_alloc_particle_pool(SpritePool *pool, s32 capacity);
void gear_model_free_particle_pool(SpritePool *pool);
void gear_model_reset_particle_pool(SpritePool *pool);
EffectSprite *gear_model_take_particle(SpritePool *pool, s16 semi_trans);
s32 gear_model_free_particle(SpritePool *pool, EffectSprite *particle);
void gear_model_draw_particles(SpritePool *pool, MATRIX *m, s32 steps, u32 *ot, s32 buffer);
void gear_model_mark_record_free(ColorFade *fade, s32 unused);

#endif
