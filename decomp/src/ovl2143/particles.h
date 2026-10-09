#ifndef OVL2143_PARTICLES_H
#define OVL2143_PARTICLES_H

/* Particles: textured quads with a fading colour, kept in pools, and the
 * actors' channels, ribbons traced by two points of a node and emitted as
 * particles. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* A particle (0x7c bytes): a quad of four vertices, a colour fading each
 * tick, and its quad for both buffers. */
typedef struct Particle {
    s16 x0, y0, z0, pad06;
    s16 x1, y1, z1;
    s16 projected;  /* +e: vertices are 3D, projected with the GTE */
    s16 x2, y2, z2;
    s16 age;        /* +16: -1 free */
    s16 x3, y3, z3;
    s16 lifetime;   /* +1e */
    u16 color[3];   /* +20: 10.6 fixed point */
    s16 fade[3];    /* +26: per tick */
    POLY_FT4 poly[2]; /* +2c */
} Particle;

/* A pool of particles with a spare one past the end. */
typedef struct ParticlePool {
    Particle *items;
    s16 capacity;
    s16 next;
} ParticlePool;

/* An actor's 0x70-byte channel: a ribbon traced by two points of a node,
 * emitted as particles (on screen, or in 3D when `solid`). */
typedef struct {
    s16 id;         /* +0: the node, -1: unused */
    u8 solid;       /* +2 */
    u8 semi_trans;  /* +3 */
    struct ParticlePool *pool; /* +4 */
    struct Particle *particle; /* +8: the one being extended */
    SVECTOR ends[2];           /* +c: the points in the node's space */
    union {
        struct {
            s16 x, y;
        } sxy[2][8];           /* on screen, a ring of 8 frames */
        VECTOR pos[2][2];      /* in 3D, the last two frames */
    } trail;                   /* +1c */
    s16 frame;      /* +5c */
    s16 count;      /* +5e: frames of the current particle */
    s16 max;        /* +60 */
    s16 lifetime;   /* +62 */
    u16 color[3];   /* +64 */
    s16 fade[3];    /* +6a */
} Channel;

ParticlePool *func_801E0064(ParticlePool *pool, s32 capacity);
void func_801E00DC(ParticlePool *pool);
void func_801E011C(ParticlePool *pool);
struct Particle *func_801E0248(struct ParticlePool *pool, s16 semi_trans);
s32 func_801E0354(ParticlePool *pool, Particle *particle);
void func_801E0398(ParticlePool *pool, MATRIX *m, s32 steps, u32 *ot, s32 buffer);
void func_801E0844(s16 *id, s32 unused);

#endif
