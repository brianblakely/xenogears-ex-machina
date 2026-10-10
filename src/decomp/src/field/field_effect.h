#ifndef FIELD_FIELD_EFFECT_H
#define FIELD_FIELD_EFFECT_H

/* Particle effects (800a9274-800aa9dc, field_effect.c): 64 slots (800b14b0
 * states, 800b0108 owners), each a copy of the eight template emitters at
 * 800b02cc with their particles. Events set the templates up and launch
 * effects (field_event.c). */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "field/monitor.h"

/* One particle: its state, motion and quad per draw buffer. */
typedef struct {
    s16 unk00;       /* 00: alive */
    u16 unk02;       /* 02: start delay */
    u16 unk04;       /* 04: life; set to 1 to release */
    u16 angle;       /* 06 */
    VECTOR position; /* 08 */
    VECTOR velocity; /* 18 */
    VECTOR unk28;    /* 28 */
    SVECTOR unk38;   /* 38 */
    SVECTOR unk40;   /* 40 */
    u8 unk48[4];     /* 48 */
    s8 unk4C[4];     /* 4C */
    POLY_FT4 quads[2];  /* 50: per draw buffer */
    SVECTOR corners[4]; /* A0 */
} Particle;

/* One 0x78-byte particle emitter: the eight templates at 800b02cc, and
 * copies of them per effect slot (*800c3918). The comments give each
 * member's use by its readers (800a9688, 800a99a8, 800a9f18, 800aa6b4) and,
 * in capitals, the label debug595's particle editor prints it under. */
typedef struct Record78 {
    s16 unk00;       /* 00 */
    u16 unk02;       /* 02: start delay, frames before it spawns (SWAIT) */
    u16 unk04;       /* 04: frames it spawns, 7fff lasting (EWAIT) */
    s16 count;       /* 06: particles, 0 none (MAX) */
    s32 unk08;       /* 08: launch speed (SPEED) */
    SVECTOR unk0C;   /* 0C: start point, the centre of the spawn area (SPOS) */
    SVECTOR unk14;   /* 14: end point, where the particles aim (EPOS) */
    SVECTOR unk1C;   /* 1C: gravity, added to the velocity each frame (GRAVITE) */
    s16 unk24;       /* 24: launch speed multiplier (SPEED's second value) */
    u16 unk26;       /* 26: spawn radius (SRANGE) */
    u16 unk28;       /* 28: aim spread around the end point (ERANGE) */
    u16 flags;       /* 2A: 1 a random angle (RANDROT), 6 the draw depth (SORT),
                      * 0x30 the launch frame, 0x40 spawn on a line and 0x80 on
                      * the radius's circle (RANGEMOD), 0x300 the blend (COLMODE) */
    Particle *particles; /* 2C */
    s16 unk30[8][2]; /* 30: spawn offset (x, z) per octant of the owner's facing in
                      * the view (ANGOFFS) */
    s16 unk50;       /* 50: the launch scale: 0x1000, the owner's in frame 3 */
    s16 unk52;       /* 52: the owner actor */
    s16 unk54;       /* 54: particle sprite, of field_effect_particle_sprites (SHAPE) */
    u16 unk56;       /* 56: frames between the launches of a frame's spawns (PSWAIT) */
    u16 unk58;       /* 58: particle life (PEWAIT) */
    SVECTOR unk5A;   /* 5A: particle size (SCALE) */
    SVECTOR unk62;   /* 62: particle size step per frame (SCALEOFS) */
    u8 unk6A;        /* 6A: particle colour: red (COLOR) */
    u8 unk6B;        /* 6B: green */
    u8 unk6C;        /* 6C: blue */
    u8 unk6D;
    s8 unk6E;        /* 6E: colour step per frame: red (COLOROFS) */
    s8 unk6F;        /* 6F: green */
    u8 unk70;        /* 70: blue */
    u8 unk71;
    s16 unk72;       /* 72: 801e layer actor of launch frame 1 */
    s16 unk74;       /* 74: its node */
    u16 unk76;       /* 76: particle angle without a random one (ROTANGLE) */
} Record78;

/* The templates, set up by the events (the edited and the selected one,
 * field_effect_edited_template and field_effect_template_actor, are in field/monitor.h). */
extern Record78 field_effect_templates[8]; /* the eight template emitters */
void field_effect_reset_templates(s32 value); /* reset the templates */
void field_event_set_template_pairs(s32 first); /* set four of the selected emitter template's spawn offsets */

/* An effect launch (80088674, 80088790) for ext 90 and 93. */
typedef struct FieldLaunch {
    s32 actor;       /* 2374 */
    s32 frame;       /* 2378: launch frame kind << 4 */
    s32 layer_actor; /* 237C: 801e layer actor of frame 1 */
    s32 layer_node;  /* 2380: 801e layer node of frame 1 */
    s32 record;      /* 2384: the selected emitter template (800b02cc), ext 90 */
} FieldLaunch;

extern FieldLaunch field_effect_launch;

/* The effect slots (starting and stopping an owner's effects, field_effect_start
 * and field_effect_stop_by_owner, are in field/monitor.h). */
extern u8 field_effect_slot_states[64];          /* effect slot states */
extern s16 field_effect_slot_owners[64];         /* effect slot owners, -1 free */
extern Record78 *field_effect_slot_emitters[64]; /* effect slot emitters */
extern s32 field_unread_effect_last_owner;       /* last effect owner */
void field_effect_clear_slots(void);             /* free all slots */
void field_effect_release_all_slots(void);       /* release all slots */
void field_effect_release_slot(s32 slot);        /* release a slot and its particles */
void field_effect_update_slots(void);            /* run the slots for a frame */

/* Particles. */
void field_effect_spawn_particle(Record78 *emitter, Particle *particle, s32 *spawned); /* spawn */
void field_effect_step_particle(Record78 *emitter, Particle *particle, MATRIX *view); /* step */
void field_effect_draw_particle(Particle *particle, MATRIX *view, s16 angle, s32 depth_mode, VECTOR *scale, s32 mode);
void field_effect_init_particle_quads(Particle *particle, s32 sprite, s32 abr); /* set its quads up */
s32 field_effect_add_clamped_u8(s32 value, s32 delta); /* `value` + `delta` clamped to 0..255 */

#endif
