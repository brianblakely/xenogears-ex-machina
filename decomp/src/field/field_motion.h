#ifndef FIELD_FIELD_MOTION_H
#define FIELD_FIELD_MOTION_H

/* Actor motion and collision (field_8007A44C.c, and the event moves of
 * 80097a50-80099fff in field_800854D0.c): the field update's motion
 * stages, floors and edges, contacts and triggers, the party followers'
 * movement history, platforms, and moves toward a target or along arcs. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "field/monitor.h"
#include "field.h"

/* The per-actor motion stages of the field update (8008110c); the flag they
 * test, D_800ADB98, is in field/monitor.h. */
extern s32 D_800AF858;         /* the next 801e layer entry the update visits */
extern s32 D_800ADC0C;         /* 1 once the update ran */
void func_80082620(s32 index, FieldDescriptor *descriptor, FieldActor *actor); /* additive motion */
void func_80082BB8(s32 index, FieldDescriptor *descriptor, FieldActor *actor); /* move an actor */
void func_8008399C(s32 index, FieldDescriptor *descriptor, FieldActor *actor); /* talk and touch triggers */
void func_80084158(s32 index, FieldDescriptor *descriptor, FieldActor *actor); /* contacts */
void func_80081F80(Sprite *sprite, s16 heading, FieldDescriptor *descriptor); /* planar velocity */
void func_800821F4(void *model, s32 animation, FieldDescriptor *descriptor); /* start an animation */
s32 func_8008492C(FieldActor *actor);  /* 0 when the actor may idle */
void func_80080A74(s32 index);         /* reset an actor and settle it on its floors */
void func_80080F44(s32 index);         /* create an actor */
void func_8008083C(s32 index);         /* release an actor */
s32 func_80081F5C(FieldActor *actor);  /* -1 when its bits 9-10 meet bits 3-4 of +14 */
u32 func_80080968(struct FieldActor *actor); /* the collision attribute under an actor */
void func_800A0C94(void);              /* mirror the current actor's position */
void func_800A0C4C(void);              /* set flag 0x80 on the controlled actor */
void func_8009E574(s32 x, s32 z);      /* place the current actor on its floor */
extern s32 D_800ADF64;                 /* touch latch (8008399c) */

/* Floors and the scratchpad work areas of the collision walks. */
s32 func_8007B1C4(s32 x, s32 z, s32 layer, SVECTOR *point, VECTOR *normal); /* floor triangle under x/z */
s32 func_8007D3D4(FieldActor *actor, s32 layer, s32 *floor, VECTOR *normal, s16 *triangle, s32 *upper);
s32 func_8007D8B4(s32 x, s32 y, s32 z); /* the component of largest magnitude */
s32 func_8007B694(VECTOR *v);  /* the heading of an x/z offset */
extern s32 D_800ADC10;         /* scratchpad words in use */
u32 *func_8007CD3C(s32 words); /* allocate scratchpad words */
void func_8007CD60(s32 words); /* release them */

/* The polygon check's scratchpad work area (0xb8 bytes, 80083288). */
typedef struct {
    s32 packed[4];     /* 00: projected vertices, x << 16 | z */
    s32 point;         /* 10: the queried x << 16 | z */
    SVECTOR v[4];      /* 14: transformed vertices */
    SVECTOR p;         /* 34: query point; vy receives the height */
    long flag;         /* 3C */
    MATRIX transform;  /* 40 */
    MATRIX local;      /* 60 */
    MATRIX view;       /* 80 */
    s32 lowest;        /* A0 */
    SVECTOR *vertices; /* A4 */
    u8 unkA8[4];
    s32 type;          /* AC */
    SVECTOR angles;    /* B0 */
} PolyCheck;

/* Terrain pushes. */
extern s16 D_800ADFC4[4];      /* terrain push speeds */
extern u16 D_800ADFA8[8];      /* terrain push angles */

/* An actor's link to the platform it rides (actor +110, 12 bytes). */
typedef struct PlatformLink {
    SVECTOR rotation; /* 0: platform rotation last frame */
    s16 radius;       /* 8: distance to the platform */
    s16 unkA;
} PlatformLink;

/* One 0x48-byte record of the controlled actor's movement history (32 at
 * 800b14f0, newest at 800b2360, filled downward); the followers replay it
 * (800815f0). */
typedef struct FieldHistory {
    u32 flags;          /* 00: actor +000 */
    u32 layer_flags;    /* 04: actor +004 */
    s16 position[3];    /* 08: whole x, y, z */
    s16 unk0E;
    u16 model84;        /* 10: model +84 */
    s16 unk12;          /* 12: actor +0e8 */
    s16 heading;        /* 14 */
    s16 triangle[4];    /* 16 */
    u8 unk1E[2];
    s32 model_velocity[3]; /* 20: model velocity */
    u8 unk2C[4];
    s32 unk30[3];       /* 30: actor +050 */
    u8 unk3C[4];
    u32 unk40;          /* 40: actor +014 */
    u8 layer;           /* 44 */
    u8 unk45[3];
} FieldHistory;

extern FieldHistory D_800B14F0[32];
extern s32 D_800B2360[3];      /* movement history index per party slot */
extern s32 D_800C3910;         /* history reset */
void func_800815F0(void);      /* move the party followers */
void func_80081C54(s32 index); /* record the controlled actor in the history */

/* Moves toward a target and arcs (event moves). */
#define ACTOR_ARC_STEPS(actor) (*(s16 *)&(actor)->unk0DC[4]) /* +e0: the arc's step count */
extern u16 D_800B14AC;         /* a jump's frames, two per step (800809d0) */
s32 func_800825AC(s32 from, s32 to);    /* planar distance between two actors */
s32 func_80099A04(s32 dx, s32 dy, s32 dz); /* vector length */
s32 func_80099A4C(s32 dx, s32 dz);      /* planar length */
s32 func_80099A8C(s32 x);               /* absolute value through the GTE */

#endif
