#ifndef WORLDMAP_EFFECT_H
#define WORLDMAP_EFFECT_H

/* The world map's particle effects and drifting sprites (worldmap_80083A00):
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

extern AreaObject *D_8009BCC0; /* the area data's emitters */
extern EffectSlot *D_8009BDF4;
extern void *D_8009BE1C[2];    /* particle quads, per display buffer */

void func_80088F64(void); /* clear the area objects, allocate the effect slots */
void func_80088FF4(void); /* free the effect slots */
void func_8008901C(void); /* allocate the particle quads */
void func_80089128(void); /* free them */
void func_80089160(s32 effect, SVECTOR *position, SVECTOR *angle); /* place and start a group */
void func_800894C8(s32 a); /* deactivate a group */
void func_80089514(s32 a); /* stop a group's live particles */
void func_80089748(void); /* run the emitters */
void func_80089C78(void); /* draw the particles */

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

extern Drift *D_8009D150;
extern DriftVelocity *D_8009CEB4;
extern void *D_8009D7F8[2]; /* cloud quads, per display buffer */

void func_800863E0(void); /* scatter the clouds */
void func_80086568(void); /* free them */
void func_800865A0(void); /* allocate the cloud quads */
void func_800866C8(void); /* free them */
void func_80086700(void); /* move the clouds */
void func_80086798(void); /* draw the clouds */

#endif
