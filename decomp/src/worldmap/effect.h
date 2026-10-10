#ifndef WORLDMAP_EFFECT_H
#define WORLDMAP_EFFECT_H

/* The world map's particle effects and drifting sprites (worldmap_objects_effects_party):
 * the area objects (emitters, eight per group) that spawn particles into
 * the effect slots, and the clouds that drift over the terrain. */

#include "worldmap.h"

/* Area object (0x54 bytes, 512 of them, eight per group): a particle
 * emitter. */
typedef struct AreaObject {
    s32 unk0;          /* emit timer reload */
    s32 unk4;          /* packed emit timer: low delay, high repeats */
    s16 unk8;          /* most live particles */
    s16 unkA;          /* live particles */
    s32 life;          /* 0x0C: particle life word (see EFFECT_COUNT) */
    s16 unk10;         /* emit interval */
    s16 unk12;         /* frames to the next emission */
    SVECTOR position;  /* 0x14 */
    SVECTOR angle;     /* 0x1C */
    SVECTOR unk24;     /* 0x24: emission offset */
    SVECTOR direction; /* 0x2C: target offset */
    s32 speed;         /* 0x34 */
    s16 accel[3];      /* 0x38 */
    s16 pad3E;
    u16 spread[2];     /* 0x40: offset and target distance range */
    s32 rot;           /* 0x44: packed particle rotation */
    s32 spin;          /* 0x48: packed rotation step */
    u8 rgb[3];         /* 0x4C: particle colour ... */
    u8 flags;          /* 0x4F: ... whose code byte holds the flags, 0x80 active */
    s32 fade;          /* 0x50: packed colour step */
} AreaObject;

/* Particle life word: low half frames left, high half nonzero while live. */
#define EFFECT_COUNT(slot) (((s16 *)&(slot)->timer)[0])
#define EFFECT_ENABLED(slot) (((s16 *)&(slot)->timer)[1])

/* Effect slot (0x4C bytes, 256 of them): one particle. */
typedef struct EffectSlot {
    s16 id;            /* emitting area object */
    s16 unk2;
    s32 timer;         /* 0x04: see EFFECT_COUNT, EFFECT_ENABLED */
    VECTOR position;   /* 0x08 */
    VECTOR velocity;   /* 0x18 */
    VECTOR accel;      /* 0x28 */
    s16 rot[2];        /* 0x38 */
    s16 spin[2];       /* 0x3C */
    s32 colour;        /* 0x40: packed r, g, b and the primitive code */
    s32 fade;          /* 0x44: packed signed r, g, b steps */
    s16 code;          /* 0x48: primitive code and semi-transparency */
    s16 pad4A;
} EffectSlot;

extern AreaObject *worldmap_effect_emitters; /* the area data's emitters */
extern EffectSlot *worldmap_effect_slots;
extern void *worldmap_effect_quads[2];       /* particle quads, per display buffer */

void worldmap_effects_alloc_slots(void); /* clear the area objects, allocate the effect slots */
void worldmap_effects_free_slots(void); /* free the effect slots */
void worldmap_effects_alloc_quads(void); /* allocate the particle quads */
void worldmap_effects_free_quads(void); /* free them */
void worldmap_effects_start_emitters(s32 effect, SVECTOR *position, SVECTOR *angle); /* place and start a group */
void worldmap_effects_stop_emitters(s32 a); /* deactivate a group */
void worldmap_effects_stop_particles(s32 a); /* stop a group's live particles */
void worldmap_effects_run_emitters(void); /* run the emitters */
void worldmap_effects_draw_particles(void); /* draw the particles */

/* Drifting position (0x10 bytes) and its velocity (8 bytes): the 80 clouds. */
typedef struct Drift {
    s32 x;
    s32 unk4;
    s32 z;
    s32 unkC;
} Drift;

typedef struct DriftVelocity {
    s16 dx;
    s16 unk2;
    s16 dz;
    s16 unk6;
} DriftVelocity;

extern Drift *worldmap_cloud_positions;
extern DriftVelocity *worldmap_cloud_velocities;
extern void *worldmap_cloud_quads[2]; /* cloud quads, per display buffer */

void worldmap_clouds_scatter(void); /* scatter the clouds */
void worldmap_clouds_free(void); /* free them */
void worldmap_clouds_alloc_quads(void); /* allocate the cloud quads */
void worldmap_clouds_free_quads(void); /* free them */
void worldmap_clouds_move(void); /* move the clouds */
void worldmap_clouds_draw(void); /* draw the clouds */

#endif
