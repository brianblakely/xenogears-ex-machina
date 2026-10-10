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
 * copies of them per effect slot (*800c3918). */
typedef struct Record78 {
    s16 unk00;       /* 00 */
    u16 unk02;       /* 02: start delay */
    u16 unk04;       /* 04: lifetime, 7fff lasting */
    s16 count;       /* 06: particles */
    s32 unk08;       /* 08 */
    SVECTOR unk0C;   /* 0C */
    SVECTOR unk14;   /* 14 */
    SVECTOR unk1C;   /* 1C */
    s16 unk24;       /* 24 */
    u16 unk26;       /* 26: spawn radius */
    u16 unk28;       /* 28: velocity spread */
    u16 flags;       /* 2A */
    Particle *particles; /* 2C */
    s16 unk30[8][2]; /* 30 */
    s16 unk50;       /* 50 */
    s16 unk52;       /* 52 */
    s16 unk54;       /* 54 */
    u16 unk56;       /* 56: spawn interval */
    u16 unk58;       /* 58: particle life */
    SVECTOR unk5A;   /* 5A */
    SVECTOR unk62;   /* 62 */
    u8 unk6A;        /* 6A */
    u8 unk6B;        /* 6B */
    u8 unk6C;        /* 6C */
    u8 unk6D;
    s8 unk6E;        /* 6E */
    s8 unk6F;        /* 6F */
    u8 unk70;        /* 70 */
    u8 unk71;
    s16 unk72;       /* 72 */
    s16 unk74;       /* 74 */
    u16 unk76;       /* 76: particle angle */
} Record78;

/* The templates, set up by the events (the edited and the selected one,
 * field_effect_edited_template and field_effect_template_actor, are in field/monitor.h). */
extern Record78 field_effect_templates[8]; /* the eight template emitters */
void field_effect_reset_templates(s32 value); /* reset the templates */
void field_event_set_template_pairs(s32 first); /* set the current template's +30 pairs from operands */

/* An effect launch (80088674, 80088790) for ext 90 and 93. */
typedef struct FieldLaunch {
    s32 actor;       /* 2374 */
    s32 frame;       /* 2378: launch frame kind << 4 */
    s32 layer_actor; /* 237C: 801e layer actor of frame 1 */
    s32 layer_node;  /* 2380: 801e layer node of frame 1 */
    s32 record;      /* 2384: emitter record (800b02cc) being set */
} FieldLaunch;

extern FieldLaunch field_effect_launch;

/* The effect slots (starting and stopping an owner's effects, field_effect_start
 * and field_effect_stop_by_owner, are in field/monitor.h). */
extern u8 field_effect_slot_states[64];      /* effect slot states */
extern s16 field_effect_slot_owners[64];     /* effect slot owners, -1 free */
extern Record78 *field_effect_slot_emitters[64]; /* effect slot emitters */
extern s32 field_unread_effect_last_owner;         /* last effect owner */
void field_effect_clear_slots(void);      /* free all slots */
void field_effect_release_all_slots(void);      /* release all slots */
void field_effect_release_slot(s32 slot);  /* release a slot and its particles */
void field_effect_update_slots(void);      /* run the slots for a frame */

/* Particles. */
void field_effect_spawn_particle(Record78 *emitter, Particle *particle, s32 *spawned); /* spawn */
void field_effect_step_particle(Record78 *emitter, Particle *particle, MATRIX *view); /* step */
void field_effect_draw_particle(Particle *particle, MATRIX *view, s16 angle, s32 depth_mode, VECTOR *scale, s32 mode);
void field_effect_init_particle_quads(Particle *particle, s32 sprite, s32 abr); /* set its quads up */
s32 field_effect_add_clamped_u8(s32 value, s32 delta); /* `value` + `delta` clamped to 0..255 */

#endif
