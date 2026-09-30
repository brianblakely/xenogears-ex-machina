#ifndef FIELD_FIELD_MOTION_H
#define FIELD_FIELD_MOTION_H

#include "field.h"

/* Actor movement (80097a50-80099fff): moves toward a target, walking and
 * scripted arcs. */

/* The motion view of a descriptor's model (FieldModel +00..+1f). */
typedef struct {
    s32 position[3]; /* 00 */
    s32 unk0C;       /* 0C */
    s32 velocity;    /* 10: vertical, 16.16 per step */
    s32 unk14;       /* 14 */
    s32 unk18;       /* 18 */
    Fixed gravity;   /* 1C */
} ModelMotion;

/* The step count of an actor's arc (+e0). */
#define ACTOR_ARC_STEPS(actor) (*(s16 *)&(actor)->unk0DC[4])

s16 func_8007B1C4(s32 x, s32 z, s32 layer, SVECTOR *point, VECTOR *normal);

#endif
